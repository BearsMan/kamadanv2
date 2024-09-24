#ifdef GW_HELPER_C_INC
#error "gw-helper.c is already included"
#endif
#define GW_HELPER_C_INC

#include <float.h> // FLT_EPSILON
#include <common/time.h>
#include <common/macro.h>

#include <signal.h>
#include <stdio.h>

#include "async.h"

static int   irand(int min, int max);
static float frand(float min, float max);
static float dist2(Vec2f u, Vec2f v);
static bool  equ2f(Vec2f v1, Vec2f v2);
static Vec2f lerp2f(Vec2f a, Vec2f b, float t);

#define sec_to_ms(sec) (sec * 1000)

// Ensure uint16_t* is null terminated, adding one if needed. Returns length of uint16_t string excluding null terminator.
size_t null_terminate_uint16(uint16_t* buffer, size_t buffer_len) {
    for (size_t i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0)
            return i;
    }
    buffer[buffer_len - 1] = 0;
    return buffer_len - 1;
}
// Ensure wchar_t* is null terminated, adding one if needed. Returns length of wchar_t string excluding null terminator.
size_t null_terminate_wchar(wchar_t* buffer, size_t buffer_len) {
    for (size_t i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0)
            return i;
    }
    buffer[buffer_len - 1] = 0;
    return buffer_len - 1;
}
// Ensure char* is null terminated, adding one if needed. Returns length of char string excluding null terminator.
size_t null_terminate_char(char* buffer, size_t buffer_len) {
    LogDebug("null_terminate_char: %p, %d", buffer, buffer_len);
    for (size_t i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0)
            return i;
    }
    buffer[buffer_len - 1] = 0;
    return buffer_len - 1;
}

// Returns length of char string excluding null terminator.
// from_buffer MUST be a null terminated string.
static int wchar_to_char(const wchar_t* from_buffer, char* to_buffer, size_t to_buffer_length) {
    setlocale(LC_ALL, "en_US.utf8");
#pragma warning(suppress : 4996)
#if 0
    //wcstombs(pIdentifier->Description, desc.Description, MAX_DEVICE_IDENTIFIER_STRING);
    int len = WideCharToMultiByte(CP_UTF8, 0, from, -1, NULL, 0, NULL, NULL);
    if (len == 0 || len > (int)max_len)
        return -1;
    WideCharToMultiByte(CP_UTF8, 0, from, -1, to, len, NULL, NULL);
    return (int)null_terminate_char(to, max_len);
#else
    int from_buffer_len = wcslen(from_buffer);
    if (to_buffer_length < from_buffer_len * sizeof(wchar_t)) {
        LogError("wchar_to_char: Out buffer length %d is not big enough; need %d", to_buffer_length, from_buffer_len * sizeof(wchar_t));
        return -1; // char array should be twice the length of the from_buffer
    }
    LogDebug("wchar_to_char: wcstombs(%p, %p, %d)", to_buffer, from_buffer, to_buffer_length);
    int len = (int)wcstombs(to_buffer, from_buffer, to_buffer_length);
    if (len < 0) {
        LogError("wchar_to_char: wcstombs failed %d", len);
        return len;
    }
    return (int)null_terminate_char(to_buffer, to_buffer_length);
#endif
}
// Returns length of wchar_t string excluding null terminator.
static int uint16_to_wchar(const uint16_t* from_buffer, wchar_t* to_buffer, size_t to_buffer_length) {
    size_t written = 0;
    to_buffer[0] = 0;
    for (written = 0; written < to_buffer_length - 1; written++) {
        to_buffer[written] = from_buffer[written];
        if (!from_buffer[written])
            break;
    }
    return (int)null_terminate_wchar(to_buffer, to_buffer_length);
}
// Returns length of char string excluding null terminator.
static int uint16_to_char(const uint16_t* from_buffer, char* to_buffer, const size_t to_buffer_length) {
    int result = -1;
    if (!to_buffer_length)
        return result;
    size_t tmp_to_buffer_bytes = to_buffer_length * sizeof(wchar_t);
    wchar_t* tmp_to_buffer = malloc(tmp_to_buffer_bytes);
    result = uint16_to_wchar(from_buffer, tmp_to_buffer, to_buffer_length);
    if (result < 1)
        goto cleanup;
    // Result is null terminated now.
    result = wchar_to_char(tmp_to_buffer, to_buffer, to_buffer_length);
cleanup:
    free(tmp_to_buffer);
    return result;
}
static bool str_match_uint32_t(uint32_t* first, uint32_t* second) {
    size_t len1 = 0;
    while (first[len1]) len1++;
    size_t len2 = 0;
    while (first[len2]) len2++;
    if (len1 != len2)
        return false;
    for (size_t i = 0; i < len1; i++) {
        if (first[i] != second[i])
            return false;
    }
    return true;
}
static bool str_match_uint16_t(uint16_t* first, uint16_t* second) {
    size_t len1 = 0;
    while (first[len1]) len1++;
    size_t len2 = 0;
    while (first[len2]) len2++;
    if (len1 != len2)
        return false;
    for (size_t i = 0; i < len1; i++) {
        if (first[i] != second[i])
            return false;
    }
    return true;
}
static bool is_same_item(ApiItem* item1, ApiItem* item2) {
    if (item1->type != item2->type)
        return false;
    if (item1->model_id != item2->model_id)
        return false;
    uint32_t mod_struct_1[8];
    GetItemModStruct(item1->item_id, mod_struct_1, ARRAY_SIZE(mod_struct_1));
    uint32_t mod_struct_2[8];
    GetItemModStruct(item2->item_id, mod_struct_2, ARRAY_SIZE(mod_struct_2));
    if (!str_match_uint32_t(mod_struct_1, mod_struct_2))
        return false;
    return true;
}
static ApiAgent get_me() {
    ApiAgent res;
    res.agent_id = 0;
    int my_id = GetMyAgentId();
    if (my_id) 
        GetAgent(&res, my_id);
    return res;
}
static bool get_my_position(Vec2f* out) {
    ApiAgent me = get_me();
    if (!me.agent_id)
        return false;
    *out = me.position;
    return true;
}
static float get_distance_to_agent(ApiAgent* agent) {
    ApiAgent me = get_me();
    if (!me.agent_id)
        return 0.0f;
    return dist2(agent->position, me.position);
}

static bool is_dead(AgentId agent_id)
{
    AgentEffect effect = GetAgentEffects(agent_id);
    return (effect & AgentEffect_Dead) != 0;
}

static void get_all_agents(ArrayApiAgent *agents)
{
    array_clear(agents);
    size_t count = GetAgents(NULL, 0);
    if (count) {
        array_reserve(agents, count);
        agents->size = GetAgents(agents->data, agents->capacity);
    }
}

static bool get_trader_item(ApiItem* like_item, bool is_sell, ApiItem* return_item, ApiItem* merchant_items, size_t merchant_items_length) {
    if (!merchant_items || !merchant_items_length)
        return false;
    for (size_t i = 0; i < merchant_items_length; i++) {
        if (is_sell && GetItemLocation(merchant_items[i].item_id, NULL) == BagEnum_Invalid)
            continue;
        if (!is_sell && GetItemLocation(merchant_items[i].item_id, NULL) != BagEnum_Invalid)
            continue;
        if (!is_same_item(like_item, &merchant_items[i]))
            continue;
        *return_item = merchant_items[i];
        return true;
    }
    return false;

}
static uint32_t get_quote(ApiItem* item, msec_t timeout_ms) {
    AsyncState state;
    uint32_t price = 0;
    async_get_quote(&state, item->item_id,&price, timeout_ms);
    while (!async_check(&state)) {
        time_sleep_ms(16);
    }
    if (state.result != ASYNC_RESULT_OK) {
        LogInfo("Failed to get quote for item %d - result code %d", item->item_id, state.result);
    }
    return price;
}
static bool go_to_npc(ApiAgent* agent, msec_t timeout_ms) {
    if (!(agent && agent->agent_id))
        return false;
    msec_t elapsed = 0;
    msec_t next_movement_check = 3000;
    Vec2f my_position;
    if (!get_my_position(&my_position)) {
        LogError("Failed to get my position");
        return false;
    }
    AsyncState state;
    async_go_to_npc(&state, agent->agent_id, timeout_ms);


    while (!async_check(&state)) {
        elapsed += 16;
        time_sleep_ms((int)elapsed);
        if (elapsed > next_movement_check) { 
            Vec2f new_position;
            if (!get_my_position(&new_position)) {
                LogError("Failed to get my position");
                async_cancel(&state);
                return false;
            }
            if (new_position.x == my_position.x && new_position.y == my_position.y) {
                LogDebug("Stopped moving, possibly blocked by a roaming npc; re-triggering go_to_npc");
                async_cancel(&state);
                return go_to_npc(agent, timeout_ms - elapsed);
            }
            my_position = new_position;
            next_movement_check += 500;
        }
    }
    if (state.result != ASYNC_RESULT_OK) {
        LogInfo("Failed to go to npc %d - result code %d", agent->agent_id, state.result);
    }
    return state.result == ASYNC_RESULT_OK;
}
static bool wait_for_merchant_items(msec_t deadlock) {
    msec_t deadlock_break = time_get_ms() + deadlock;
    ApiItem merchant_items[512];
    AgentId my_id = GetMyAgentId();
    while (!GetMerchantItems(merchant_items, 512)) {
        if ((time_get_ms() >= deadlock_break) || !GetIsIngame() || is_dead(my_id)) {
            return false;
        }
    }
    return true;
}
static bool go_to_trader_deadlock(ApiAgent* agent, msec_t timeout_ms) {
    return go_to_npc(agent, timeout_ms) && wait_for_merchant_items(timeout_ms);
}

static bool move_wait_coarse(float x, float y, float randomness, float dist_to_reach)
{
    Vec2f dest;
    if (randomness != 0.f) {
        dest.x = x + frand(-randomness, randomness);
        dest.y = y + frand(-randomness, randomness);
    } else {
        dest.x = x;
        dest.y = y;
    }
    MoveToPoint(dest);
    AgentId my_id = GetMyAgentId();
    if (!my_id)
        return false;
    msec_t deadlock_break = time_get_ms() + 60000;
    Vec2f pos = GetAgentPos(my_id);
    float dist = 9999.0f;
    int sleeps = 0;
    while (dist > dist_to_reach) {
        if ((time_get_ms() >= deadlock_break) || !GetIsIngame() || is_dead(my_id)) 
            return false;
        pos = GetAgentPos(my_id);
        if (sleeps % 100 == 0)
            printf("My position is %.2f, %.2f\n", pos.x, pos.y);
        dist = MoveTo_dist2(pos, dest);
        time_sleep_ms(1);
        sleeps++;
    }
    bool success = dist < dist_to_reach;
    pos = GetAgentPos(my_id);
    return success;
}

static bool move_wait(float x, float y, float randomness)
{
    return move_wait_coarse(x, y, randomness, FLT_EPSILON);
}

static void move_rand(float x, float y, float randomness)
{
    if (randomness != 0.f) {
        x += frand(-randomness, randomness);
        y += frand(-randomness, randomness);
    }
    MoveToCoord(x, y);
}

static int irand(int min, int max)
{
    assert(0 < (max - min) < RAND_MAX);
    int range = max - min;
    return (rand() % range) + min;
}

static float frand(float min, float max)
{
    assert(0 < (max - min) < RAND_MAX);
    float range = max - min;
    float random = (float)rand() / RAND_MAX;
    return (random * range) + min;
}

static float dist2(Vec2f u, Vec2f v)
{
    float dx = u.x - v.x;
    float dy = u.y - v.y;
    return sqrtf((dx * dx) + (dy * dy));
}

static bool equ2f(Vec2f v1, Vec2f v2)
{
    if (v1.x != v2.x)
        return false;
    if (v1.y != v2.y)
        return false;
    return true;
}

static Vec2f lerp2f(Vec2f a, Vec2f b, float t)
{
    Vec2f pos;
    pos.x = ((1.f - t) * a.x) + (t * b.x);
    pos.y = ((1.f - t) * a.y) + (t * b.y);
    return pos;
}

static ApiAgent get_agent_with_model_id(int npc_id)
{
    ApiAgent res;
    res.agent_id = 0;
    if (!npc_id) return res;
    ArrayApiAgent agents = {0};
    get_all_agents(&agents);
    for (size_t i = 0; i < agents.size; i++) {
        ApiAgent *agent = &agents.data[i];
        if (agent->type != AgentType_Living)
            continue;
        int id = (int)GetNpcIdOfAgent(agent->agent_id);
        if (id && (id == npc_id)) {
            res = *agent;
            break;
        }
    }
    array_reset(&agents);
    return res;
}

static bool get_closest_npc_to_coords(float x,float y, ApiAgent* out)
{
    Vec2f location = { x, y };
    ApiAgent agent;
    agent.agent_id = 0;
    Vec2f my_position;
    if (!get_my_position(&my_position)) {
        LogError("get_closest_npc_to_coords: Failed to get my position");
        return false;
    }
    if (dist2(location, my_position) > 5000.0f) {
        LogDebug("get_closest_npc_to_coords: coords %.0f %.0f are beyond compass range",x,y);
        return false;
    }    
    ArrayApiAgent agents;
    array_init(&agents);
    get_all_agents(&agents);
    
    float closest_dist = 9999.0f;
    ApiAgent* closest = 0;
    for (size_t i = 0; i < agents.size; i++) {
        ApiAgent* tmpAgent = &agents.data[i];
        if (tmpAgent->type != AgentType_Living)
            continue;
        uint32_t npc_id = GetNpcIdOfAgent(tmpAgent->agent_id);
        if (!npc_id)
            continue;
        float distance = dist2(location, tmpAgent->position);
        if (distance < closest_dist) {
            closest = tmpAgent;
            closest_dist = distance;
        }
    }
    if (closest) {
        *out = *closest;
        array_reset(&agents);
        return true;
    }
    array_reset(&agents);
    return false;
}

static ApiAgent get_agent_with_model_id_deadlock(int model_id, msec_t deadlock)
{
    msec_t deadlock_break = time_get_ms() + deadlock;
    ApiAgent agent = get_agent_with_model_id(model_id);
    while (!agent.agent_id) {
        time_sleep_ms(16);
        if (time_get_ms() >= deadlock_break)
            break;
        if (!GetIsIngame())
            break;
        agent = get_agent_with_model_id(model_id);
    }
    return agent;
}

static int count_foes_in_range(float range, Vec2f pos, uint32_t npc_id)
{
    int count = 0;
    if (!npc_id) return 0;
    ApiAgent* agent;
    ArrayApiAgent agents = {0};
    get_all_agents(&agents);
    array_foreach(agent, &agents) {
        if (agent->type != AgentType_Living) continue;
        if (agent->effects & AgentEffect_Dead) continue;
        uint32_t id = GetNpcIdOfAgent(agent->agent_id);
        if (id != npc_id) continue;
        if (dist2(pos, agent->position) > range)
            continue;
        count++;
    }
    return count;
}

static ArrayApiItem get_items_in_bag(BagEnum bag)
{
    ArrayApiItem items = {0};
    size_t count = GetBagItems(bag, NULL, 0);
    array_reserve(&items, count);
    items.size = GetBagItems(bag, items.data, items.capacity);
    return items;
}

static int count_bag_free_slot(BagEnum bag)
{
    size_t capacity = GetBagCapacity(bag);
    ArrayApiItem items = get_items_in_bag(bag);
    size_t count = capacity - items.size;
    array_reset(&items);
    return (int)count;
}

static int count_inventory_free_slot(void)
{
    int free_slot = 0;
    free_slot += count_bag_free_slot(BagEnum_Backpack);
    free_slot += count_bag_free_slot(BagEnum_BeltPouch);
    free_slot += count_bag_free_slot(BagEnum_Bag1);
    free_slot += count_bag_free_slot(BagEnum_Bag2);
    return free_slot;
}

static int count_bag_item(BagEnum bag, uint32_t model_id)
{
    ArrayApiItem items = get_items_in_bag(bag);
    int count = 0;
    ApiItem *item;
    array_foreach(item, &items) {
        if (item->model_id == model_id)
            count += item->quantity;
    }
    array_reset(&items);
    return count;
}

static int count_inventory_item(int model_id)
{
    int count = 0;
    count += count_bag_item(BagEnum_Backpack, model_id);
    count += count_bag_item(BagEnum_BeltPouch, model_id);
    count += count_bag_item(BagEnum_Bag1, model_id);
    count += count_bag_item(BagEnum_Bag2, model_id);
    return count;
}

static bool get_item_with_model_id(uint32_t model_id, ApiItem *item)
{
    ApiItem items[20];
    size_t  n_items;
    for (BagEnum bag_id = BagEnum_Backpack; bag_id <= BagEnum_Bag2; bag_id++) {
        n_items = GetBagItems(bag_id, items, ARRAY_SIZE(items));
        for (size_t i = 0; i < n_items; i++) {
            if (items[i].model_id == model_id) {
                if (item) *item = items[i];
                return true;
            }
        }
    }
    return false;
}

static ArrayApiQuest get_all_quests(void)
{
    ArrayApiQuest quests = {0};
    size_t count = GetQuests(NULL, 0);
    if (!count) return quests;
    array_reserve(&quests, count);
    quests.size = GetQuests(quests.data, quests.capacity);
    return quests;
}

static ApiQuest get_quest_with_id(uint32_t quest_id)
{
    ApiQuest quest;
    quest.quest_id = 0;
    ArrayApiQuest quests = get_all_quests();
    ApiQuest *it;
    array_foreach(it,&quests) {
        if (it->quest_id == quest_id)
            return *it;
    }
    array_reset(&quests);
    return quest;
}

static bool player_has_quest(int quest_id)
{
    ApiQuest quest = get_quest_with_id(quest_id);
    return (quest.quest_id != 0);
}

static bool wait_quest_deadlock(int quest_id, msec_t deadlock)
{
    msec_t deadlock_break = time_get_ms() + deadlock;
    while (!player_has_quest(quest_id)) {
        time_sleep_ms(16);
        if (time_get_ms() >= deadlock_break)
            return false;
    }
    return true;
}

static void ping_sleep(msec_t ms)
{
    time_sleep_ms((unsigned int)(GetPing() + ms));
}

#if 0
static void buy_rare_material_item(Item *item)
{
    if (!item->quote_price) return;

    TransactionInfo send_info = {0};
    TransactionInfo recv_info = {0};

    recv_info.item_count = 1;
    recv_info.item_ids[0] = item->item_id;
    recv_info.item_quants[0] = 1;

    // @TODO
    // BuyMaterials(TRANSACT_TYPE_TraderBuy, item->quote_price, &send_info, 0, &recv_info);
}
#endif

static int get_rare_material_trader_id(int map_id)
{
    switch(map_id) {
    case 4:
    case 5:
    case 6:
    case 52:
    case 176:
    case 177:
    case 178:
    case 179:
        return 205;
    case 275:
    case 276:
    case 359:
    case 360:
    case 529:
    case 530:
    case 537:
    case 538:
        return 192;
    case 109:
        return 1997;
    case 193:
        return 3621;
    case 194:
    case 250:
    case 857:
        return 3282;
    case 242:
        return 3281;
    case 376:
        return 5388;
    case 398:
    case 433:
        return 5667;
    case 414:
        return 5668;
    case 424:
        return 5387;
    case 438:
        return 5613;
    case 49:
        return 2038;
    case 491:
    case 818:
        return 4723;
    case 449:
        return 4729;
    case 492:
        return 4722;
    case 638:
        return 6760;
    case 640:
        return 6759;
    case 641:
        return 6060;
    case 642:
        return 6045;
    case 643:
        return 6386;
    case 644:
        return 6385;
    case 652:
        return 6228;
    case 77:
        return 3410;
    case 81:
        return 2083;
    default:
        return 0;
    }
}

static int wait_map_loading(int map_id, msec_t timeout_ms);
static int travel_wait(int map_id, District district, uint16_t district_number)
{
    if ((GetMapId() == map_id) && (GetDistrict() == district)
        && (GetDistrictNumber() == district_number)) {
        return 0;
    }
    Travel(map_id, district, district_number);
    return wait_map_loading(map_id, 20000);
}

static int random_travel(int map_id)
{
    District districts[] = {
        DISTRICT_ASIA_KOREAN,
        DISTRICT_ASIA_CHINESE,
        DISTRICT_ASIA_JAPANESE
    };

    int r = irand(0, ARRAY_SIZE(districts));
    District district = districts[r];
    Travel(map_id, district, 0);
    return wait_map_loading(map_id, 20000);
}

static int travel_gh_safe(void)
{
    // @Enhancement: Theoricly, they could add a gh, but yeah...
    static int gh_map_ids[] = {
        4, 5, 6, 52, 176, 177, 178, 179, 275, 276, 359, 360, 529, 530, 537, 538
    };
    int map_id = GetMapId();
    for (int i = 0; i < ARRAY_SIZE(gh_map_ids); i++) {
        if (map_id == gh_map_ids[i])
            return true;
    }
    uint32_t guild_id = GetMyGuildId();
    TravelHall(guild_id);
    return wait_map_loading(0, 20000);
}

static ArrayApiPlayer get_all_players(void)
{
    ArrayApiPlayer players = {0};
    size_t count = GetPlayers(NULL, 0);
    if (!count) return players;
    array_reserve(&players, count);
    players.size = GetPlayers(players.data, players.capacity);
    return players;
}

static bool get_player_with_name(const wchar_t *name, ApiPlayer *player)
{
    size_t length;
    uint16_t buffer[20];
    ArrayApiPlayer players = get_all_players();

    ApiPlayer *it;
    bool match = true;
    array_foreach(it, &players) {
        match = true;
        length = GetPlayerName(it->player_id, buffer, ARRAY_SIZE(buffer));
        for (size_t i = 0; name[i] && i < length && match; i++) {
            match = name[i] == buffer[i];
        }
        if (match) {
            if (player) *player = *it;
            array_reset(&players);
            return true;
        }
    }

    array_reset(&players);
    return false;
}

static int get_skill_position(uint32_t skill_id)
{
    uint32_t skill_ids[8] = {0};
    GetSkillbar(skill_ids, 0);
    for (size_t i = 0; i < 8; i++) {
        if (skill_ids[i] == skill_id)
            return (int)(i + 1);
    }
    return 0;
}

static bool use_skill_wait(int skill_id, AgentId target, msec_t deadlock)
{
    msec_t deadlock_break = time_get_ms() + deadlock;
    int position = get_skill_position(skill_id);
    if (position == 0)
        return false;
    UseSkill(skill_id, target);
    ping_sleep(500); // We should set the skill as casting even if we didn't get the confirmation from the server
    while (GetSkillCasting(position, NULL)) {
        time_sleep_ms(16);
        if (deadlock && (time_get_ms() >= deadlock_break))
            return false;
        if (!GetIsIngame())
            return false;
    }
    return true;
}

static void set_hard_mode(void)
{
    if (GetDifficulty() != Difficulty_Hard)
        SetDifficulty(Difficulty_Hard);
}

static void set_normal_mode(void)
{
    if (GetDifficulty() != Difficulty_Normal)
        SetDifficulty(Difficulty_Normal);
}

static int32_t get_guild_faction(void)
{
    uint32_t guild_id = GetMyGuildId();
    if (!guild_id) return 0;
    return GetGuildFaction(guild_id, NULL);
}

static int wait_map_loading(int map_id, msec_t timeout_ms)
{
    AsyncState state;
    async_wait_map_loading(&state, timeout_ms);

    unsigned int current_ms = 0;
    while (!async_check(&state)) {
        time_sleep_ms(16);
    }
    if (state.result != ASYNC_RESULT_OK) {
        return state.result;
    }
    // @Cleanup:
    // We shouldn't have that here.
    current_ms = 0;
    while (!GetMyAgentId()) {
        time_sleep_ms(current_ms += 16);
        if (current_ms > (int)timeout_ms) {
            return ASYNC_RESULT_TIMEOUT; // Timeout
        }
    }

    return GetMapId() == map_id ? ASYNC_RESULT_OK : ASYNC_RESULT_WRONG_VALUE;
}
