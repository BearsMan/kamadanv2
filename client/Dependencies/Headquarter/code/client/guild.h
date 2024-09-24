#ifdef CORE_GUILD_H
#error "guild.h included more than once"
#endif
#define CORE_GUILD_H

typedef struct GuildMember {
    uint64_t last_login_utc;
    uint32_t status;
    uint32_t member_type;

    struct kstr_hdr account_name;
    uint16_t account_name_buffer[20];
    struct kstr_hdr player_name;
    uint16_t player_name_buffer[20];
} GuildMember;
typedef array(GuildMember) ArrayGuildMember;

typedef struct Guild {
    uint32_t        guild_id;
    struct uuid     guild_uuid;

    FactionType     allegiance;
    uint32_t        faction_pts;

    struct kstr_hdr tag;
    uint16_t        tag_buffer[32];
    struct kstr_hdr name;
    uint16_t        name_buffer[64];

    ArrayGuildMember members;
} Guild;
typedef array(Guild) ArrayGuild;

typedef struct GuildMemberUpdate {
    bool pending;
    uint32_t status;
    uint8_t  member_type;
    uint32_t minutes_since_login;
    struct kstr_hdr player_name;
    uint16_t player_name_buffer[20];
} GuildMemberUpdate;

Guild *get_guild_safe(World *world, uint32_t guild_id);
void init_guildmember_update(GuildMemberUpdate *gmu);
void reset_guildmember_update(GuildMemberUpdate *gmu);
GuildMember *complete_guildmember_update(World *world, struct kstr account_name);

void api_make_guild(ApiGuild* dest, Guild* src)
{
    dest->guild_id = src->guild_id;
    kstr_hdr_write(&src->name, dest->name, ARRAY_SIZE(dest->name));
    kstr_hdr_write(&src->tag, dest->tag, ARRAY_SIZE(dest->tag));
}

void api_make_guild_member(ApiGuildMember* dest, GuildMember* src)
{
    dest->status = src->status;
    dest->type = src->member_type;
    dest->last_login_utc = src->last_login_utc;
    
    kstr_hdr_write(&src->account_name, dest->account_name, ARRAY_SIZE(dest->account_name));
    if (dest->status == 1) {
        kstr_hdr_write(&src->player_name, dest->player_name, ARRAY_SIZE(dest->player_name));
    } else {
        dest->player_name[0] = 0;
    }
}
