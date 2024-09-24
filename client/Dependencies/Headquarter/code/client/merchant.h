#ifdef CORE_MERCHANT_H
#error "merchant.h included more than once"
#endif
#define CORE_MERCHANT_H

typedef enum TransactionType {
    TransactionType_MerchantBuy = 1,
    TransactionType_CollectorBuy,
    TransactionType_CrafterBuy,
    TransactionType_WeaponsmithCustomize,
    TransactionType_MerchantSell = 11,
    TransactionType_TraderBuy,
    TransactionType_TraderSell,
    TransactionType_UnlockRunePriestOfBalth = 15
} TransactionType;

typedef struct TransactionInfo {
    uint32_t gold;
    size_t   item_count;
    uint32_t item_ids[16];
    uint32_t item_quants[16];
} TransactionInfo;

typedef struct QuoteInfo {
    uint32_t unk1;
    size_t   item_count;
    uint32_t item_ids[16];
} QuoteInfo;
