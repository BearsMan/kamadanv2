#ifndef __STDC__
# define __STDC__ 1
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <locale.h>
#include <stdbool.h>
#include "../Dependencies/curl-8.10.1/include/curl/curl.h"

#define HEADQUARTER_RUNTIME_LINKING
#include <client/constants.h>
#include <client/Headquarter.h>
#include <common/time.h>
#include <common/thread.h>

#include "gw-helper.c"
#ifdef _WIN32
# define DllExport __declspec(dllexport)
#else
# define DllExport
#endif

#include <signal.h>
#include <stdio.h>

#define MapID_Ascalon                   148
#define MapID_Kamadan                   449
#define MapID_Kamadan_Halloween         818
#define MapID_Kamadan_Wintersday        819
#define MapID_Kamadan_Canthan_New_Year  820

// Kamadan NPCs
#define Durmand_Location                -11120.0f,12070.0f
#define RuneTrader_Location             -10235.00,16557.0f
#define WeaponTrader_Location           -11270.0f,8785.0f
#define MaterialTrader_Location         -11442.0f,9092.0f
#define RareMaterialTrader_Location     -10997.0f,10022.0f
#define Crier_Location                  -8469.0f,12144.0f

#define Kamadan_DistrictNumber          1
#define Kamadan_GetQuotes               1

// Combines chat message with party search message types
typedef enum MessageType {
    Chat_Alliance = 0,
    Chat_Allies = 1,
    Chat_GWCA1 = 2,
    Chat_All = 3,
    Chat_GWCA2 = 4,
    Chat_Moderator = 5,
    Chat_Emote = 6,
    Chat_Warning = 7,
    Chat_GWCA3 = 8,
    Chat_Guild = 9,
    Chat_Global = 10,
    Chat_Group = 11,
    Chat_Trade = 12,
    Chat_Advisory = 13,
    Chat_Whisper = 14,
    PartySearch_Hunting = 100,
    PartySearch_Mission = 101,
    PartySearch_Quest = 102,
    PartySearch_Trade = 103,
    PartySearch_Guild = 104
} MessageType;

typedef array(char) CharArray;

// Material quote prices.
typedef struct PriceQuote {
    uint32_t mod_struct[8];
    uint32_t model_id;
    uint32_t price;
    uint64_t timestamp;
    bool is_sell;
} PriceQuote;
typedef array(PriceQuote) PriceQuotes;

static volatile bool  running;
static bool need_to_send_quotes = false;
static struct thread  bot_thread;
static PluginObject* bot_module;
static thread_event_t game_started_event;
static char ServerUrl[128];
static char trade_url[164];
static char message_url[164]; // New generic url
static char local_chat_url[164];
static char whisper_url[164];
static char favor_url[164];
static char account_uuid[128];

static int required_map_id = MapID_Kamadan;
static uint16_t required_district_number = 1;
static bool need_to_get_quotes = false;

static uint64_t age_request_sent = 0;
static uint64_t last_message_received = 0;
static uint64_t last_material_quote_check = 0;

static uint64_t get_material_quotes_interval = 1800; // Interval in seconds

static PriceQuotes quotes;
static wchar_t player_name[20];

static CallbackEntry on_travel_start_entry, on_travel_end_entry;
static CallbackEntry chat_message_entry;
static CallbackEntry on_trade_price_entry;
static CallbackEntry on_party_advertisement_entry;
static CallbackEntry on_dialog_button_entry;

int materials[] = { 921, 925, 929, 933, 934, 940, 946, 948, 953, 954, 955, 922, 923, 926, 927, 928, 930, 931, 932, 935, 936, 937, 938, 939, 941, 942, 943, 944, 945, 949, 950, 951, 952, 956, 6532, 6533 };

static void exit_with_status(const char* state, int exit_code)
{
    LogCritical("%s (code=%d)", state, exit_code);
    exit(exit_code);
}

static void send_server_message(const char* url, const MessageType type, const char* sender_utf8, const char* message_utf8);

static int print_mod_struct(char* buffer, int buffer_len, uint32_t* mod_struct) {
    int written = 0;
    for (size_t i = 0; mod_struct[i] != 0;i++) {
        written += snprintf(&buffer[written], buffer_len - written, "%X", mod_struct[i]);
    }
    buffer[written] = 0;
    return written;
}

static bool in_correct_outpost() {
    if (!GetIsIngame())
        return false;
    switch (GetMapId()) {
    case MapID_Ascalon:
        return required_map_id == MapID_Ascalon && GetDistrict() == DISTRICT_AMERICAN && GetDistrictNumber() == required_district_number;
    case MapID_Kamadan:
    case MapID_Kamadan_Halloween:
    case MapID_Kamadan_Wintersday:
    case MapID_Kamadan_Canthan_New_Year:
        return required_map_id == MapID_Kamadan && GetDistrict() == DISTRICT_AMERICAN && GetDistrictNumber() == required_district_number;
    }
    return false;
}
static bool is_pre_searing() {
    switch (GetMapId()) {
    case MapID_Ascalon: // Ascalon
    case 162: // Regent Valley
    case 164: // Ashford Abbey
    case 165: // Foible's Fair
    case 166: // Fort Ranik
        return true;
    }
    return false;
}
static CURL* init_curl(const char* url) {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);
    static struct curl_slist* slist = NULL;
    if (!slist) {
        slist = curl_slist_append(slist, "Accept:");
        slist = curl_slist_append(slist, is_pre_searing() ? "Host: ascalon.gwtoolbox.com" : "Host: kamadan.gwtoolbox.com");
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    return curl;
}

static bool send_quotes_to_server(bool include_modstruct) {
    bool success = false;
    // NB: References to CharArray.size in here do NOT include null terminating char, but one will be there from snprintf!
    CharArray buy_quotes_buffer;
    array_init(&buy_quotes_buffer);
    CharArray sell_quotes_buffer;
    array_init(&sell_quotes_buffer);
    CharArray json_buffer;
    array_init(&json_buffer);
    CURL* curl = 0;
    PriceQuote* quote = 0;
    char mod_struct_string[128];
    array_foreach(quote, &quotes) {
        if (!quote->timestamp)
            continue;
        if(include_modstruct) print_mod_struct(mod_struct_string, sizeof(mod_struct_string), quote->mod_struct);
        CharArray* buffer = 0;
        if (quote->is_sell) {
            buffer = &sell_quotes_buffer;
        }
        else {
            buffer = &buy_quotes_buffer;
        }
        array_reserve(buffer, 1024);
        int written = -1;
        if (include_modstruct && strlen(mod_struct_string)) {
            array_reserve(buffer, buffer->size + 256);
            written = snprintf(&buffer->data[buffer->size], buffer->capacity,
                "%c{\"m\":\"%d-%s\",\"p\":%d,\"t\":%lld}",
                buffer->size > 0 ? ',' : ' ', quote->model_id, mod_struct_string, quote->price, (long long)quote->timestamp);
        }
        else {
            array_reserve(buffer, buffer->size + 64);
            written = snprintf(&buffer->data[buffer->size], buffer->capacity,
                "%c{\"m\":%d,\"p\":%d,\"t\":%lld}",
                buffer->size > 0 ? ',' : ' ', quote->model_id, quote->price, (long long)quote->timestamp);
        }
        assert(written > 0);
        buffer->size += (size_t)written;

    }
    if (!(sell_quotes_buffer.size || buy_quotes_buffer.size)) {
        LogError("No quotes fetched!");
        goto cleanup;
    }
    size_t json_buffer_len = 32 + sell_quotes_buffer.size + buy_quotes_buffer.size;
    array_reserve(&json_buffer, json_buffer_len);
    int written = snprintf(json_buffer.data, json_buffer.capacity, "json={\"buy\":[%s],\"sell\":[%s]}",
        buy_quotes_buffer.size ? buy_quotes_buffer.data : "", 
        sell_quotes_buffer.size ? sell_quotes_buffer.data : "");
    assert(written > 0);
    json_buffer.size = (size_t)written;
    LogDebug("JSON length is %d", json_buffer.size);
    /* get a curl handle */
    char url[164];
    snprintf(url, 164, "%s/trader_quotes", ServerUrl);
    curl = init_curl(url);
    if (!curl) {
        LogError("init_curl() failed");
        goto cleanup;
    }
    LogInfo("Sending trader quotes to %s:", url);
    //printf("%s",json_buffer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_buffer.data);
    CURLcode res = curl_easy_perform(curl);
    success = res == CURLE_OK;
    if (!success) {
        LogError("curl_easy_perform() failed: %s", curl_easy_strerror(res));
        goto cleanup;
    }
cleanup:
    array_reset(&sell_quotes_buffer);
    array_reset(&buy_quotes_buffer);
    array_reset(&json_buffer);
    if(curl) curl_easy_cleanup(curl);
    if (success) need_to_send_quotes = false;
    return success;
}
static bool go_to_npc_kama(float x, float y) {
    ApiAgent npc;
    bool valid = get_closest_npc_to_coords(x, y, &npc);
    Vec2f coords_vec2f = { x, y };
    const float range = 255.f;
    valid = valid && dist2(coords_vec2f, npc.position) < range;
    Vec2f my_position;
    if (!get_my_position(&my_position)) {
        LogError("go_to_npc_kama: Failed to get my position");
        return false;
    }

    if (!valid) {
        LogDebug("Failed to find trader within %.2f of %.2f, %.2f (1)", range, x, y);
        const Vec2f intermediaries[] = {
            {Crier_Location},
            {Durmand_Location}
        };
        const size_t len = sizeof(intermediaries) / sizeof(*intermediaries);
        ApiAgent intermediary;
        ApiAgent to_use;
        to_use.position.x = to_use.position.y = 0.f;
        to_use.agent_id = 0;
        float intermediary_dist = 9999.f;
        for (size_t i = 0; i < len; i++) {
            if (!get_closest_npc_to_coords(intermediaries[i].x, intermediaries[i].y, &intermediary))
                continue;
            float dist = dist2(my_position, intermediary.position);
            if (!to_use.agent_id || dist < intermediary_dist) {
                intermediary_dist = dist;
                to_use = intermediary;
            }
        }
        if (!to_use.agent_id) {
            LogError("Failed to get intermediary agent.");
            return false;
        }
        LogDebug("Moving to intermediary agent %d @ %.2f, %.2f", to_use.agent_id, to_use.position.x, to_use.position.y);
        if (!go_to_npc(&to_use, 30000)) {
            LogError("Failed to move to interact with intermediary");
            return false;
        }
        valid = get_closest_npc_to_coords(x, y, &npc) && dist2(coords_vec2f, npc.position) < range;
        if (!valid) {
            LogError("Failed to find trader within %.2f of %.2f, %.2f (2)", range, x, y);
            return false;
        }
    }
    LogDebug("Found trader with agent id %d @ %.2f, %.2f, moving to location",npc.agent_id, npc.position.x, npc.position.y);
    if (!go_to_npc(&npc, 30000)) {
        LogError("Failed to move to interact with trader");
        return false;
    }
    return true;
}
static bool go_to_trader_kama(float x, float y) {
    return go_to_npc_kama(x, y) && wait_for_merchant_items(5000);
}
static int get_quotes() {
    int ret = 0;
    ApiItem merchant_items[360];
    size_t merchant_items_length = GetMerchantItems(merchant_items, ARRAY_SIZE(merchant_items));
    if (!merchant_items_length) {
        LogError("Invalid merchant item size of %d", merchant_items_length);
        return ret;
    }
    // Buy quotes.
    LogInfo("Fetching quotes for %d items", merchant_items_length);
    ApiItem* item;
    char mod_struct_string[128];
    array_clear(&quotes);
    for (size_t i = 0; i < merchant_items_length; i++) {
        item = &merchant_items[i];
        PriceQuote q;
        q.is_sell = GetItemLocation(item->item_id, NULL) != BagEnum_Invalid;
        q.price = get_quote(item,3000);
        if (!q.price) continue; // Price is 0 when we failed to get a quote
        q.model_id = item->model_id;
        q.timestamp = (uint64_t)time(NULL);
        GetItemModStruct(item->item_id, q.mod_struct, ARRAY_SIZE(q.mod_struct));
        print_mod_struct(mod_struct_string, ARRAY_SIZE(mod_struct_string), q.mod_struct);
        LogDebug("Price %d found for %d, %s", q.price, q.model_id, mod_struct_string);
        array_add(&quotes, q);
    }
    LogInfo("Gotted quotes");
    return (int)array_size(&quotes);
}
static bool get_trader_quotes() {
    LogDebug("Moving to weapons trader");
    if (!go_to_npc_kama(WeaponTrader_Location))
        exit_with_status("Failed to move to weapons trader",0);
    // Common material trader
    LogDebug("Fetching quotes from common material trader");
    if (!go_to_trader_kama(MaterialTrader_Location))
        exit_with_status("Failed to move to common material trader", 0);
    if (get_quotes()) send_quotes_to_server(false);
    // Rare material trader
    LogDebug("Fetching quotes from rare material trader");
    if (!go_to_trader_kama(RareMaterialTrader_Location))
        exit_with_status("Failed to move to rare material trader", 0);
    if (get_quotes()) send_quotes_to_server(false);
    
    // Rune trader
    LogDebug("Fetching quotes from rune trader");
    if (!go_to_trader_kama(RuneTrader_Location))
        exit_with_status("Failed to move to rune trader", 0);
    if (get_quotes()) send_quotes_to_server(true);
    array_clear(&quotes);
    go_to_npc_kama(WeaponTrader_Location);
    return true;
}
static void on_party_advertisement(Event* event, void* param) {
    assert(event->type == EventType_PartySearchAdvertisement);
    last_message_received = time_get_ms() / 1000;
    //last_message_received = time_get_ms() / 1000;
    char* sender_utf8 = 0;
    char* message_utf8 = 0;
    if (!in_correct_outpost())
        return; // This isn't a new advertisement - map loads feed of current adverts.
    MessageType message_type = (MessageType)(event->PartySearchAdvertisement.search_type + 100);
    switch (message_type) {
    case PartySearch_Trade:
        break; // Only log trade party search adverts
    default:
        return; // No other channels supported.
    }
    if (event->PartySearchAdvertisement.district_number == GetDistrictNumber())
        return; // Don't log it if its from my district
    if (!event->PartySearchAdvertisement.message.length) {
        LogError("on_party_advertisement: No message length");
        goto cleanup;
    }
    if (!event->PartySearchAdvertisement.sender.length) {
        LogError("on_party_advertisement: No sender length");
        goto cleanup;
    }

    size_t message_utf8_bytes = (event->PartySearchAdvertisement.message.length + 1) * sizeof(wchar_t);
    message_utf8 = malloc(message_utf8_bytes);
    int message_written = uint16_to_char(event->PartySearchAdvertisement.message.buffer, message_utf8, message_utf8_bytes);
    if (message_written < 1) {
        LogError("Failed to uint16_to_char message");
        goto cleanup;
    }
    size_t sender_utf8_bytes = (event->PartySearchAdvertisement.sender.length + 1) * sizeof(wchar_t);
    sender_utf8 = malloc(sender_utf8_bytes);
    int sender_written = uint16_to_char(event->PartySearchAdvertisement.sender.buffer, sender_utf8, sender_utf8_bytes);
    if (sender_written < 1) {
        LogError("Failed to uint16_to_char sender");
        goto cleanup;
    }
    send_server_message(message_url, message_type, sender_utf8, message_utf8);
cleanup:
    if (sender_utf8) free(sender_utf8);
    if (message_utf8) free(message_utf8);
}

static void on_dialog_button(Event* event, void* param) {
    assert(event->type == EventType_DialogButton);
    if (event->DialogButton.button_id == 0x7F) {
        // See merchant wares - weapon trader would have a quest which would block merchant window.
        SendDialog(event->DialogButton.button_id);
    }
}

static void on_chat_message(Event* event, void * param)
{
    assert(event->type == EventType_ChatMessage);
    
    last_message_received = time_get_ms() / 1000;

    char* url = message_url;
    char* sender_char = 0;
    int sender_char_bytes = 0;
    char* message_char = 0;
    int message_char_bytes = 0;
    wchar_t* sender_wchar = 0;
    int sender_wchar_bytes = 0;
    wchar_t* message_wchar = 0;
    int message_wchar_bytes = 0;
    MessageType message_type = (MessageType)event->ChatMessage.channel;

    switch (message_type) {
    case Chat_Trade:
    case Chat_All:
    case Chat_Whisper:
    case Chat_Global:
        break;
    default:
        return; // No other channels supported.
    }

    
    if (!event->ChatMessage.message.length) {
        LogError("on_chat_message: No message length");
        goto cleanup;
    }
    if (!event->ChatMessage.sender.length) {
        LogError("on_chat_message: No sender length");
        goto cleanup;
    }

    
    int message_length = (int)event->ChatMessage.message.length;
    int sender_length = (int)event->ChatMessage.sender.length;
    message_wchar_bytes = (message_length + 1) * sizeof(*message_wchar);
    message_wchar = malloc(message_wchar_bytes);
    if (!message_wchar) {
        LogError("on_chat_message: message_wchar = malloc(%d) Failed!", message_wchar_bytes);
        goto cleanup;
    }
    message_length = uint16_to_wchar(event->ChatMessage.message.buffer, message_wchar, sender_wchar_bytes / sizeof(*message_wchar));
    if (message_length < 1) {
        LogError("on_chat_message: Failed to uint16_to_wchar on message");
        goto cleanup;
    }
    // NB: message_wchar is now properly null terminated
    sender_wchar_bytes = (sender_length + 1) * sizeof(*sender_wchar);
    sender_wchar = malloc(sender_wchar_bytes);
    if (!sender_wchar) {
        LogError("on_chat_message: sender_wchar = malloc(%d) Failed!",sender_wchar_bytes);
        goto cleanup;
    }
    sender_length = uint16_to_wchar(event->ChatMessage.sender.buffer, sender_wchar, sender_wchar_bytes / sizeof(*sender_wchar));
    if (sender_length < 1) {
        LogError("on_chat_message: Failed to uint16_to_wchar on sender");
        goto cleanup;
    }
    // NB: sender_wchar is now properly null terminated
    // Calculate offset for non-encoded message
    int message_start = 0;
    switch (message_wchar[message_start]) {
    case 0x8102:
        message_start += 3;
        break;
    case 0x108:
        message_start += 2;
        break;
    }
    // Now we know the offset, convert wchar_t into char 
    message_length -= message_start;
    // char bytes should be at least twice as much as the wchar bytes
    message_char_bytes = message_wchar_bytes * sizeof(*message_wchar);
    message_char = malloc(message_char_bytes);
    if (!message_char) {
        LogError("on_chat_message: message_char = malloc(%d) Failed!", message_char_bytes);
        goto cleanup;
    }
    message_length = wchar_to_char(&message_wchar[message_start], message_char, message_char_bytes / sizeof(*message_char));
    if (message_length < 1) {
        LogError("on_chat_message: Failed to wchar_to_char on message");
        goto cleanup;
    }
    // char bytes should be at least twice as much as the wchar bytes
    sender_char_bytes = sender_wchar_bytes * sizeof(*sender_wchar);
    sender_char = malloc(sender_char_bytes);
    if (!sender_char) {
        LogError("on_chat_message: sender_char = malloc(%d) Failed!", sender_char_bytes);
        goto cleanup;
    }
    sender_length = wchar_to_char(sender_wchar, sender_char, sender_char_bytes / sizeof(*sender_char));
    if (sender_length < 1) {
        LogError("on_chat_message: Failed to wchar_to_char on sender");
        goto cleanup;
    }
    // Trim any trailing 0x1 chars.
    while (message_length > -1 && message_char[message_length - 1] == 0x1) {
        message_char[message_length - 1] = 0;
        message_length--;
    }
    if (message_length < 1) {
        LogError("on_chat_message: Empty message after trimming 0x1");
        goto cleanup;
    }
    // Special commands
    if (event->ChatMessage.channel == Channel_Whisper && wcscmp(sender_wchar, player_name) == 0) {
        
        LogInfo("Message from myself received");
        goto cleanup;
    }
    if (event->ChatMessage.channel == Channel_Whisper && wcscmp(message_wchar, L"quotes") == 0) {
        LogInfo("Quotes command received");
        last_material_quote_check = 0;
        goto cleanup;
    }
    //LogDebug("%s: %s\n", sender_char, message_char);
    send_server_message(url, message_type, sender_char, message_char);

cleanup:
    if (sender_char) free(sender_char);
    if (message_char) free(message_char);
    if (sender_wchar) free(sender_wchar);
    if (message_wchar) free(message_wchar);
}
// Forward a message on to the server (sender and message must be null terminated)
static void send_server_message(const char* url, const MessageType type, const char* sender_utf8, const char* message_utf8) {
    size_t sender_length = strlen(sender_utf8);
    size_t message_length = strlen(message_utf8);
    /* get a curl handle */
    CURL* curl = 0;
    CharArray cmsg_buf;
    array_init(&cmsg_buf);
    CharArray post_buffer;
    array_init(&post_buffer);
    array_reserve(&cmsg_buf, 16 + sender_length + message_length);

    int written = snprintf(&cmsg_buf.data[cmsg_buf.size], cmsg_buf.capacity, "[TRADE] %s: %s", sender_utf8, message_utf8);
    if (written < 1) 
        goto snprintf_error;
    cmsg_buf.size += written;
    curl = init_curl(url);
    if (!curl) {
        LogError("send_server_message: curl_easy_init() failed");
        goto cleanup;
    }
    char* escaped_sender = curl_easy_escape(curl, sender_utf8, (int)sender_length);
    if (!escaped_sender) {
        LogError("Failed to curl_easy_escape sender_utf8");
        LogError("%s", sender_utf8);
        goto cleanup;
    }
    char* escaped_message = curl_easy_escape(curl, message_utf8, (int)message_length);
    if (!escaped_message) {
        LogError("Failed to curl_easy_escape message_utf8");
        LogError("%s", message_utf8);
        goto cleanup;
    }
    array_reserve(&post_buffer, 16 + strlen(escaped_sender) + strlen(escaped_message));
    written = snprintf(post_buffer.data, post_buffer.capacity, "t=%d&s=%s&m=%s", type, escaped_sender, escaped_message);
    if (written < 1) 
        goto snprintf_error;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_buffer.data);
    CURLcode res = curl_easy_perform(curl);
    bool success = res == CURLE_OK;
    if (!success) {
        const char* errStr = curl_easy_strerror(res);
        array_reserve(&cmsg_buf, cmsg_buf.size + 16 + strlen(errStr));
        written = snprintf(&cmsg_buf.data[cmsg_buf.size], cmsg_buf.capacity, " [Error: %s]", errStr);
        if (written < 1)
            goto snprintf_error;
        cmsg_buf.size += written;
        goto cleanup;
    }
    array_reserve(&cmsg_buf, cmsg_buf.size + 12);
    written = snprintf(&cmsg_buf.data[cmsg_buf.size], cmsg_buf.capacity, " [Sent OK]");
    if (written < 1)
        goto snprintf_error;
    cmsg_buf.size += written;
    
cleanup:
    if (cmsg_buf.size)
        LogDebug("%s", cmsg_buf.data);
    array_reset(&cmsg_buf);
    array_reset(&post_buffer);
    if (curl) curl_easy_cleanup(curl);
    return;
snprintf_error:
    LogError("send_server_message: buffer overflow snprintf");
    goto cleanup;
}

// Based on current character, choose which preferred district/map
static void set_runtime_vars() {
    
    if (wcscmp(player_name, L"Larius Logsalot") == 0) {
        required_map_id = MapID_Ascalon;
        required_district_number = 1;
    }
    else if (wcscmp(player_name, L"Lorraine Logsalot") == 0) {
        required_map_id = MapID_Kamadan;
        required_district_number = 1;
    }
    else if (wcscmp(player_name, L"Logaine Loggington") == 0) {
        required_map_id = MapID_Kamadan;
        required_district_number = 2;
    }
    else if (wcscmp(player_name, L"Lauticius Logsalot") == 0) {
        required_map_id = MapID_Kamadan;
        required_district_number = 1;
    }
    else if (wcscmp(player_name, L"Tyressa Kryptblade") == 0) {
        required_map_id = MapID_Kamadan;
        required_district_number = 1;
    }
    else {
        LogError("set_runtime_vars: Unrecognised character name %ls", player_name);
        raise(SIGTERM);
    }

    need_to_get_quotes = required_map_id == MapID_Kamadan && required_district_number == 1 && Kamadan_GetQuotes;
}

static int main_bot(void* param)
{
    running = true;

    LogInfo("HQ hooks done");
    array_init(&quotes);
    curl_global_init(CURL_GLOBAL_ALL);
    long wait = 0;
    long wait_timeout_seconds = 60;
    LogInfo("Waiting for game load");
    while (!GetIsIngame()) {
        if (wait > 1000 * wait_timeout_seconds) {
            exit_with_status("Failed to wait for ingame after 10s", 1);
        }
        wait += 500;
        time_sleep_ms(500);
    }
    time_sleep_ms(2000);

    sprintf(ServerUrl, "http://127.0.0.1"); // Can change later maybe
    //sprintf(ServerUrl, "http://kamadan.gwtoolbox.com"); // Can change later maybe
    //sprintf(ServerUrl, "http://local.kamadan.com"); // Can change later maybe
    snprintf(message_url, 164, "%s/message", ServerUrl);
    snprintf(trade_url, 164, "%s/party_search", ServerUrl);
    snprintf(trade_url, 164, "%s/trade", ServerUrl);
    snprintf(local_chat_url, 164, "%s/local", ServerUrl);
    snprintf(whisper_url, 164, "%s/whisper", ServerUrl);
    snprintf(favor_url, 164, "%s/favor", ServerUrl);

    char player_name_ret[28];
    int player_name_len = 0;
    msec_t deadlock = time_get_ms() + 5000;
    while (!(player_name_len = GetCharacterName(player_name_ret, ARRAY_SIZE(player_name_ret)))) {
        if (time_get_ms() > deadlock) {
            LogError("Couldn't get current character name");
            goto cleanup;
        }
        time_sleep_ms(50);
    };
    int ok = GetAccountUuid(account_uuid, sizeof(account_uuid) / sizeof(*account_uuid));
    if (ok < 10) {
        LogError("Failed to get account uuid");
        goto cleanup;
    }
    LogInfo("Account UUID is %s", account_uuid);
    
    for (int i = 0; i < player_name_len; i++) {
        if (player_name_ret[i] == 0)
            continue;
        player_name[i] = player_name_ret[i];
    }
    player_name[player_name_len] = 0;
    LogInfo("Player name is %ls", player_name);
    set_runtime_vars();

    CallbackEntry_Init(&chat_message_entry, on_chat_message, NULL);
    RegisterEvent(EventType_ChatMessage, &chat_message_entry);

    CallbackEntry_Init(&on_party_advertisement_entry, on_party_advertisement, NULL);
    RegisterEvent(EventType_PartySearchAdvertisement, &on_party_advertisement_entry);

    CallbackEntry_Init(&on_dialog_button_entry, on_dialog_button, NULL);
    RegisterEvent(EventType_DialogButton, &on_dialog_button_entry);

    uint64_t elapsed_seconds = last_message_received = time_get_ms() / 1000;
    int not_in_game_tally = 0;
    unsigned int sleep_ms = 500;
    unsigned int stale_ping_s = 120; // Seconds to wait before sending message to myself to ensure chat connection
    while (running) {
        time_sleep_ms(sleep_ms);
        if (!GetIsIngame() || !GetIsConnected()) {
            not_in_game_tally++;
            if (not_in_game_tally * sleep_ms > 5000) {
                LogError("GetIsIngame() returned false for 5 seconds, client may have disconnected");
                goto cleanup;
            }
            continue;
        }
        if (!in_correct_outpost()) {
            LogInfo("Zoning into outpost");
            int res = 0;
            size_t retries = 4;
            for (size_t i = 0; i < retries && !in_correct_outpost(); i++) {
                LogInfo("Travel attempt %d of %d", i + 1, retries);
                res = travel_wait(required_map_id, DISTRICT_AMERICAN, required_district_number);
                if (res == 38) {
                    retries = 50; // District full, more retries
                }
            }
            if (!in_correct_outpost()) {
                exit_with_status("Couldn't travel to outpost", 1);
            }
            LogInfo("I should be in outpost %d %d %d, listening for chat messages", GetMapId(), GetDistrict(), GetDistrictNumber());
        }
        not_in_game_tally = 0;
        elapsed_seconds = time_get_ms() / 1000;
        if (elapsed_seconds > last_message_received + stale_ping_s && last_message_received > age_request_sent) {
            LogInfo("No message received for %d seconds, sending /whisper", stale_ping_s);
            age_request_sent = elapsed_seconds;
            SendChat(Channel_Group, "ping");
            //SendWhisper(player_name_ret, "ping");
        }
        if(age_request_sent > last_message_received && elapsed_seconds > age_request_sent + stale_ping_s) {
            LogError("/whisper timeout after %d seconds, client may have disconnected", stale_ping_s);
            goto cleanup;
        }
        if (need_to_get_quotes && (!last_material_quote_check || elapsed_seconds > last_material_quote_check + get_material_quotes_interval)) {
            LogInfo("Fetching material quotes");
            get_trader_quotes();
            LogInfo("Done material quotes");
            last_material_quote_check = time_get_ms() / 1000;
        }
    }
cleanup:

    UnRegisterEvent(&chat_message_entry);
    UnRegisterEvent(&on_party_advertisement_entry);
    UnRegisterEvent(&on_dialog_button_entry);
    curl_global_cleanup();
    raise(SIGTERM);
    return 0;
}
DllExport void PluginUnload(PluginObject* obj)
{
    running = false;
}
DllExport bool PluginEntry(PluginObject* obj)
{
    thread_create(&bot_thread, main_bot, NULL);
    thread_detach(&bot_thread);
    obj->PluginUnload = &PluginUnload;
    return true;
}

DllExport void on_panic(const char *msg)
{
    exit_with_status("Error", 1);
}
int main(void) {
    printf("Hello world");
    return 0;
}
