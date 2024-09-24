#ifdef CORE_AUTH_C
#error "auth.c included more than once"
#endif
#define CORE_AUTH_C

void HandleAccountSettings(Connection *conn, size_t psize, Packet *packet)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
        uint32_t n_data;
        uint8_t  data[2048];
    } AccountSettings;
#pragma pack(pop)

    assert(packet->header == AUTH_SMSG_ACCOUNT_SETTINGS);
    assert(sizeof(AccountSettings) == psize);

    GwClient *client = cast(GwClient *)conn->data;
    AccountSettings *pack = cast(AccountSettings *)packet;
    assert(client);
    (void)pack;
}

void HandleRequestResponse(Connection *conn, size_t psize, Packet *packet)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
        uint32_t code;
    } ErrorMessage;
#pragma pack(pop)

    assert(packet->header == AUTH_SMSG_REQUEST_RESPONSE);
    assert(sizeof(ErrorMessage) == psize);

    GwClient *client = cast(GwClient *)conn->data;
    ErrorMessage *pack = cast(ErrorMessage *)packet;
    assert(client);

    AsyncType type = AsyncType_None;
    for (size_t i = 0; i < array_size(&client->requests); i++) {
        AsyncRequest *request = &array_at(&client->requests, i);
        if (request->trans_id == pack->trans_id) {
            type = request->type;
            array_remove(&client->requests, i);
            break;
        }
    }

    LogDebug("HandleRequestResponse: {async_type: %d, trans_id: %lu, code: %lu}", type, pack->trans_id, pack->code);
    const char *error_s = get_error_s(pack->code);
    if (pack->code != 0) {
        LogDebug("(Code=%03d) %s", pack->code, error_s);
    }
    Event params;
    Event_Init(&params, EventType_AuthError);
    params.AuthError.code = pack->code;
    params.AuthError.type = type;
    broadcast_event(&client->event_mgr, &params);
    switch (type) {
        case AsyncType_None:
            break;
        case AsyncType_AccountLogin:
            ContinueAccountLogin(client, pack->code);
            break;
        case AsyncType_ChangeCharacter:
            ContinueChangeCharacter(client, pack->code);
            break;
        case AsyncType_PlayCharacter:
            ContinuePlayCharacter(client, pack->code);
            break;
        case AsyncType_SendHardwareInfo:
            ContinueSendHardwareInfo(client, pack->code);
            break;
        default:
            LogError("Unknow AsyncType: %d", type);
    }
}

void HandleServerReponse(Connection *conn, size_t psize, Packet *packet)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
        uint32_t code;
    } ServerReponse;
#pragma pack(pop)

    assert(packet->header == AUTH_SMSG_SERVER_RESPONSE);
    assert(sizeof(ServerReponse) == psize);

    GwClient *client = cast(GwClient *)conn->data;
    ServerReponse *pack = cast(ServerReponse *)packet;
    assert(client);
    (void)pack;
}

void HandleSessionInfo(Connection *conn, size_t psize, Packet *packet)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        uint32_t server_salt;
        uint32_t unk0;
        uint32_t unk1;
    } SessionInfo;
#pragma pack(pop)

    assert(packet->header == AUTH_SMSG_SESSION_INFO);
    assert(sizeof(SessionInfo) == psize);

    GwClient *client = cast(GwClient *)conn->data;
    SessionInfo *pack = cast(SessionInfo *)packet;
    assert(client);

    client->static_salt = pack->server_salt;
    client->state = AwaitAccountConnect;
}

void HandleAccountInfo(Connection *conn, size_t psize, Packet *packet)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        uint32_t trans_id;
        uint32_t territory;
        uint32_t unk0;
        uint8_t unk1[8];
        uint8_t unk2[8];
        uint8_t account_uuid[16];
        uint8_t character_uuid[16];
        uint32_t unk3;
        uint32_t n_unk4;
        uint8_t unk4[200];
        uint8_t eula_accepted;
        int32_t unk5;
    } AccountInfo;
#pragma pack(pop)

    assert(packet->header == AUTH_SMSG_ACCOUNT_INFO);
    assert(sizeof(AccountInfo) == psize);

    GwClient *client = cast(GwClient *)conn->data;
    AccountInfo *pack = cast(AccountInfo *)packet;
    assert(client);

    struct uuid char_uuid;
    uuid_dec_le(pack->account_uuid, &client->uuid);
    uuid_dec_le(pack->character_uuid, &char_uuid);

    for (size_t idx = 0; idx < client->characters.size; ++idx) {
        Character *ch = &client->characters.data[idx];
        if (uuid_equals(&ch->uuid, &char_uuid)) {
            LogInfo("Current character: %.*ls", ch->name.length, ch->name_buffer);
            client->current_character_idx = idx;
            break;
        }
    }
}

void HandleCharacterInfo(Connection *conn, size_t psize, Packet *packet)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
        uint8_t  uuid[16];
        uint32_t unk0;
        uint16_t name[20];
        uint32_t n_extended;
        uint8_t  extended[64];
    } CharacterInfo;
#pragma pack(pop)

    assert(packet->header == AUTH_SMSG_CHARACTER_INFO);
    assert(sizeof(CharacterInfo) == psize);

    GwClient *client = cast(GwClient *)conn->data;
    CharacterInfo *pack = cast(CharacterInfo *)packet;
    assert(client);

    // PrintBytes(p->name, pack->extended.data, 64);
    Character *character = array_push(&client->characters, 1);
    if (!character) {
        LogError("Couldn't create a new character");
        return;
    }

    init_character(character);
    kstr_hdr_read(&character->name, pack->name, ARRAY_SIZE(pack->name));
    uuid_dec_le(pack->uuid, &character->uuid);

    size_t min_size = offsetof(CharacterSettings, h0021);
    if (pack->n_extended < min_size) {
        LogWarn(
            "Extended info in `CharacterInfo` is smaller (%zu bytes) than minimum expected (%zu bytes)",
            pack->n_extended,
            min_size
        );
        return;
    }

    memcpy(&character->settings, pack->extended, pack->n_extended);
}

void AuthSrv_HeartBeat(Connection *conn, msec_t tick)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        uint32_t tick;
    } HeartBeat;
#pragma pack(pop)

    HeartBeat packet = NewPacket(AUTH_CMSG_HEARTBEAT);
    packet.tick = tick & 0xFFFFFF;
    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_AskServerResponse(Connection *conn, uint32_t trans_id)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
    } AskServerResponse;
#pragma pack(pop)

    AskServerResponse packet = NewPacket(AUTH_CMSG_ASK_SERVER_RESPONSE);
    packet.trans_id = trans_id;
    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_ComputerInfo(Connection *conn)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        uint16_t username[32];
        uint16_t pcname[32];
    } ComputerInfo;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t version;
        uint8_t  hash[16];
    } ComputerHash;
#pragma pack(pop)

    ComputerInfo info = NewPacket(AUTH_CMSG_SEND_COMPUTER_INFO);

    DECLARE_KSTR(pcname, 32);
    DECLARE_KSTR(username, 32);

    kstr_read_ascii(&pcname, "Wyatt-PC", 8);
    kstr_read_ascii(&username, "Wyatt", 5);

    kstr_write(&pcname, info.pcname, ARRAY_SIZE(info.pcname));
    kstr_write(&username, info.username, ARRAY_SIZE(info.username));

    ComputerHash hash = NewPacket(AUTH_CMSG_SEND_COMPUTER_HASH);
    hash.version = options.game_version;
    memcpy(hash.hash, "\x19\x0D\xB0\x37\xE6\x05\x13\x6B\x86\x18\x66\x28\x45\x7E\xDD\xB5", 16);

    SendPacket(conn, sizeof(info), &info);
    SendPacket(conn, sizeof(hash), &hash);
}

void AuthSrv_HardwareInfo(Connection *conn)
{
    // @Remark: Holy fuck c++ is retarded. Why the fuck it force you to add a null-terminated ?
    uint8_t hash[/* 16 */] = "\x9B\x99\x78\x8D\x2F\x01\xFA\x4F\x99\x82\xD3\xB8\xBD\x83\xCD\xF6";
    uint8_t info[/* 92 */] = "\x03\x00\x5C\x00\x04\x00\xDE\x03\x46\x0D\xC3\x06\x47\x65\x6E\x75\x69\x6E\x65\x49\x6E\x74\x65\x6C\x06\x02\x00\x00\x04\x00\x00\x00\xC0\x11\x00\x00\x60\x11\x00\x00\xDE\x10\x00\x00\xC6\x07\x20\x90\x55\x6E\x6B\x6E\x6F\x77\x6E\x20\x44\x65\x76\x69\x63\x65\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x55\x6E\x6B\x6E\x6F\x77\x6E\x20\x56\x65\x6E\x64\x6F\x72\x00\x00\x00\x00\x00\x00";

#pragma pack(push, 1)
    typedef struct {
        Header header;
        uint32_t n_info;
        uint8_t info[92];
        uint8_t hash[16];
    } HardwareInfo;
#pragma pack(pop)

    HardwareInfo packet = NewPacket(AUTH_CMSG_SEND_HARDWARE_INFO);
    packet.n_info = 92;
    memcpy(packet.info, info, 92);
    memcpy(packet.hash, hash, 16);

    SendPacket(conn, sizeof(packet), &packet);
}

// 0 = main town
// 1 = main explorable
// 2 = guild hall
// 3 = outpost/mission outpost
// 4 = mission explorable
// 5 = arena outpost
// 6 = arena explorable
// 7 = HA outpost
// 11 = character creation
void AuthSrv_RequestInstance(Connection *conn, uint32_t trans_id,
    uint32_t map_id, uint32_t type, uint32_t region, uint32_t district, uint32_t language)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
        uint32_t map_type;
        uint32_t map_id;
        uint32_t region;
        uint32_t district;
        uint32_t language;
    } CharacterPlayInfo;
#pragma pack(pop)

    CharacterPlayInfo packet = NewPacket(AUTH_CMSG_REQUEST_GAME_INSTANCE);
    packet.trans_id = trans_id;
    packet.map_type = type;
    packet.map_id = map_id;
    packet.region = region;
    packet.district = district;
    packet.language = language;

    client->state = AwaitGameServerInfo;

    LogDebug("AuthSrv_RequestInstance: map %d, district_region %d, district_number %d, language %d", packet.map_id, packet.region, packet.district, packet.language);

    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_ChangeCharacter(Connection *conn, uint32_t trans_id, struct kstr *name)
{
#pragma pack(push, 1)
    typedef struct {
        Header   header;
        uint32_t trans_id;
        uint16_t name[20];
    } ChangeCharacter;
#pragma pack(pop)

    assert(name != NULL);
    assert(name->length != 0);

    ChangeCharacter packet = NewPacket(AUTH_CMSG_CHANGE_PLAY_CHARACTER);
    packet.trans_id = trans_id;

    // @Remark: @Cleanup:
    // I'm not sure Guild Wars expect the nul-terminating character.
    // Probably not, but to be confirmed.
    size_t max_length = ARRAY_SIZE(packet.name) - 1;
    if (name->length > max_length) {
        LogError("Trying to write %zu characters in a buffer of %u",
            name->length, max_length);
        return;
    }

    memcpy(packet.name, name->buffer, name->length * 2);
    packet.name[name->length] = 0;

    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_SetPlayerStatus(Connection *conn, PlayerStatus status)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        int32_t status;
    } SetPlayerStatus;
#pragma pack(pop)

    SetPlayerStatus packet = NewPacket(AUTH_CMSG_SET_PLAYER_STATUS);
    packet.status = status;
    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_AddFriend(Connection* conn, const uint16_t* _name)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        int32_t h0004;
        int32_t h0008;
        uint16_t name[20];
        uint16_t account[20];
    } AddFriend;
#pragma pack(pop)

    AddFriend packet = NewPacket(AUTH_CMSG_FRIEND_ADD);

    DECLARE_KSTR(name, ARRAY_SIZE(packet.name));
    kstr_read(&name, _name, ARRAY_SIZE(packet.name));

    kstr_write(&name, packet.name, ARRAY_SIZE(packet.name));
    kstr_write(&name, packet.account, ARRAY_SIZE(packet.account));

    packet.h0004 = 1;
    packet.h0008 = 1;

    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_SendPacket35(Connection *conn)
{
#pragma pack(push, 1)
    typedef struct {
        Header header;
        int32_t status;
    } Packet35;
#pragma pack(pop)

    Packet35 packet = NewPacket(35);
    packet.status = 0;
    SendPacket(conn, sizeof(packet), &packet);
}

void AuthSrv_RegisterCallbacks(Connection *conn)
{
    array_init(&conn->handlers);
    array_resize(&conn->handlers, AUTH_SMSG_COUNT);

    MsgHandler *handlers = conn->handlers.data;

    handlers[AUTH_SMSG_REQUEST_RESPONSE]        = HandleRequestResponse;
    handlers[AUTH_SMSG_SERVER_RESPONSE]         = HandleServerReponse;

    handlers[AUTH_SMSG_SESSION_INFO]            = HandleSessionInfo;
    handlers[AUTH_SMSG_ACCOUNT_INFO]            = HandleAccountInfo;
    handlers[AUTH_SMSG_CHARACTER_INFO]          = HandleCharacterInfo;
    handlers[AUTH_SMSG_GAME_SERVER_INFO]        = HandleGameServerInfo;
    handlers[AUTH_SMSG_ACCOUNT_SETTINGS]        = HandleAccountSettings;

    handlers[AUTH_SMSG_FRIEND_UPDATE_INFO]      = HandleFriendUpdateInfo;
    handlers[AUTH_SMSG_FRIEND_UPDATE_STATUS]    = HandleFriendUpdateStatus;
    handlers[AUTH_SMSG_FRIEND_STREAM_END]       = HandleFriendStreamEnd;
    handlers[AUTH_SMSG_FRIEND_UPDATE_LOCATION]  = HandleFriendUpdateLocation;

    handlers[AUTH_SMSG_WHISPER_RECEIVED]        = HandleWhisperReceived;

    conn->last_tick_time = 0;
}

bool init_auth_connection(GwClient *client, const char *host)
{
    Connection *conn = &client->auth_srv;

    conn->data = client;
    if (host != NULL) {
        char buffer[256];

        // @Robustness: It doesn't support IPv6 and it won't give any error
        // if such an address is passed in arguments.
        safe_strcpy(buffer, sizeof(buffer), host);
        char *port = strrchr(buffer, ':');
        if (!port) port = "6112";

        // @Enhancement: We should print an error if he doesn't find the host.
        if (!IPv4ToAddr(buffer, port, &conn->host)) {
            LogError("Couldn't resolve the host address '%s'", host);
            return false;
        }
    }

    client->state = AwaitAuthServer;
    if (!AuthSrv_Connect(conn)) {
        LogInfo("Auth handshake failed !");
        client->state = AwaitNothing;
        return false;
    }

    if (!conn->secured) {
        LogError("Couldn't connect to Auth serveur");
        client->state = AwaitNothing;
        return false;
    }

    assert(client->state == AwaitAuthServer);
    client->state = AwaitSessionInfo;
    AuthSrv_ComputerInfo(conn);

    return true;
}
