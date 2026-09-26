// Layout físico de um bloco de bucket, usado pela organização hash da
// Parte 1 (ver Seção 7.1 do relatório: função de hashing, número de
// buckets, tratamento de colisões e overflow).
//
// Cada bloco de bucket é dividido em:
//   [ cabeçalho de 16 bytes | slots de ArticleRecord, um atrás do outro ]
//
// O cabeçalho guarda quantos slots estão ocupados e, se o bloco encheu,
// o índice do próximo bloco de overflow encadeado (ou kNoOverflow se não
// houver overflow). Isso é o que permite resolver colisões que excedem a
// capacidade de um único bloco.

#ifndef HASH_BUCKET_HPP
#define HASH_BUCKET_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "disk_record.hpp"

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 4096
#endif
constexpr std::size_t kBucketBlockSize = BLOCK_SIZE;

// Sentinela: "não há bloco de overflow encadeado a partir deste".
constexpr std::uint32_t kNoOverflow = 0xFFFFFFFFu;

// Cabeçalho de controle no início de cada bloco de bucket. Ocupa
// exatamente 16 bytes.
struct BucketBlockHeader {
    std::uint32_t next_overflow = kNoOverflow;
    std::uint16_t occupied = 0;
    std::uint8_t reserved[10] = {};
};
static_assert(sizeof(BucketBlockHeader) == 16,
              "BucketBlockHeader deve ocupar exatamente 16 bytes");

// Quantos ArticleRecord cabem em um bloco de bucket, após reservar o
// cabeçalho. Com BLOCK_SIZE=4096 dá exatamente 4, sem resto; com
// BLOCK_SIZE=8192 dá 8, com 16 bytes de resto (0,2% do bloco).
constexpr std::size_t kRecordsPerBucketBlock =
    (kBucketBlockSize - sizeof(BucketBlockHeader)) / sizeof(ArticleRecord);

static_assert(kRecordsPerBucketBlock > 0,
              "BLOCK_SIZE pequeno demais para caber pelo menos 1 registro "
              "apos o cabecalho do bucket");

// Representação em memória de um bloco de bucket já decodificado.
struct BucketBlock {
    BucketBlockHeader header;
    std::array<ArticleRecord, kRecordsPerBucketBlock> slots;

    bool cheio() const { return header.occupied >= kRecordsPerBucketBlock; }
    bool tem_overflow() const { return header.next_overflow != kNoOverflow; }
};

// Serializa um BucketBlock para um buffer de exatamente kBucketBlockSize
// bytes — o buffer que será passado para blockio::write_block(). Qualquer
// folga entre os registros e o fim do bloco (caso de BLOCK_SIZE=8192) é
// zerada.
inline void pack_block(const BucketBlock& block, void* buffer) {
    auto* p = static_cast<std::byte*>(buffer);
    std::memset(p, 0, kBucketBlockSize);
    std::memcpy(p, &block.header, sizeof(BucketBlockHeader));
    std::memcpy(p + sizeof(BucketBlockHeader), block.slots.data(),
                kRecordsPerBucketBlock * sizeof(ArticleRecord));
}

// Desserializa um buffer de kBucketBlockSize bytes (lido via
// blockio::read_block()) para um BucketBlock.
inline BucketBlock unpack_block(const void* buffer) {
    const auto* p = static_cast<const std::byte*>(buffer);
    BucketBlock block;
    std::memcpy(&block.header, p, sizeof(BucketBlockHeader));
    std::memcpy(block.slots.data(), p + sizeof(BucketBlockHeader),
                kRecordsPerBucketBlock * sizeof(ArticleRecord));
    return block;
}

// Cria um BucketBlock novo, vazio (todos os slots livres, sem overflow).
inline BucketBlock bloco_vazio() {
    BucketBlock b;
    b.header = BucketBlockHeader{};
    for (auto& slot : b.slots) slot = ArticleRecord{};  // status = vazio
    return b;
}

#endif  // HASH_BUCKET_HPP
