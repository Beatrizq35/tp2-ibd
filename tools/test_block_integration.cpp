#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "blockio.hpp"
#include "hash_bucket.hpp"

int main() {
    std::printf("BLOCK_SIZE = %zu\n", kBucketBlockSize);
    std::printf("sizeof(ArticleRecord) = %zu\n", sizeof(ArticleRecord));
    std::printf("sizeof(BucketBlockHeader) = %zu\n",
                sizeof(BucketBlockHeader));
    std::printf("registros por bloco de bucket = %zu\n",
                kRecordsPerBucketBlock);

    const std::string path = "/tmp/test_hash_bucket_integration.dat";

    // ---- Monta o bucket "0": bloco principal cheio + 1 registro extra
    //      que exige um bloco de overflow (testa a colisao/encadeamento).
    {
        blockio::BlockFile f;
        f.open(path, blockio::Mode::kTruncate);

        // Bloco principal (indice 0): preenche todos os slots.
        BucketBlock principal = bloco_vazio();
        for (std::size_t i = 0; i < kRecordsPerBucketBlock; ++i) {
            Article a;
            a.id = static_cast<int>(100 + i);
            a.titulo = "Titulo principal " + std::to_string(i);
            a.autores = "Autor " + std::to_string(i);
            a.atualizacao = "2024-01-01 00:00:00";
            a.snippet = "snippet principal " + std::to_string(i);
            principal.slots[i] = ArticleRecord::from_article(a);
        }
        principal.header.occupied =
            static_cast<std::uint16_t>(kRecordsPerBucketBlock);

        // Bloco de overflow (indice 1): 1 registro extra que nao coube.
        BucketBlock overflow = bloco_vazio();
        Article extra;
        extra.id = 999;
        extra.titulo = "Registro que foi para o overflow";
        extra.autores = "Fulano";
        extra.atualizacao = "2024-06-01 00:00:00";
        extra.snippet = "esse registro colidiu e nao coube no bloco principal";
        overflow.slots[0] = ArticleRecord::from_article(extra);
        overflow.header.occupied = 1;

        // Encadeia: bloco principal aponta para o bloco de overflow.
        std::uint64_t idx_principal = f.alloc_block();
        std::uint64_t idx_overflow = f.alloc_block();
        assert(idx_principal == 0);
        assert(idx_overflow == 1);
        principal.header.next_overflow = static_cast<std::uint32_t>(idx_overflow);

        std::vector<std::byte> buf(kBucketBlockSize);
        pack_block(principal, buf.data());
        f.write_block(idx_principal, buf.data());

        pack_block(overflow, buf.data());
        f.write_block(idx_overflow, buf.data());

        f.close();
    }

    // ---- Reabre (simulando uma nova execucao, ex: findrec depois do
    //      upload) e percorre a cadeia principal -> overflow.
    {
        blockio::BlockFile f;
        f.open(path, blockio::Mode::kRead);
        std::vector<std::byte> buf(kBucketBlockSize);

        f.read_block(0, buf.data());
        BucketBlock principal = unpack_block(buf.data());
        assert(principal.header.occupied == kRecordsPerBucketBlock);
        assert(principal.cheio());
        assert(principal.tem_overflow());
        assert(principal.header.next_overflow == 1);

        for (std::size_t i = 0; i < kRecordsPerBucketBlock; ++i) {
            assert(principal.slots[i].ocupado());
            assert(principal.slots[i].id == static_cast<int>(100 + i));
        }
        std::printf("bloco principal: OK (%zu registros, aponta para "
                    "overflow no bloco %u)\n",
                    kRecordsPerBucketBlock, principal.header.next_overflow);

        // Segue o ponteiro de overflow.
        f.read_block(principal.header.next_overflow, buf.data());
        BucketBlock overflow = unpack_block(buf.data());
        assert(overflow.header.occupied == 1);
        assert(!overflow.tem_overflow());
        assert(overflow.slots[0].ocupado());
        assert(overflow.slots[0].id == 999);
        Article a = overflow.slots[0].to_article();
        assert(a.titulo == "Registro que foi para o overflow");
        std::printf("bloco de overflow: OK (registro id=999 encontrado "
                    "apos seguir a cadeia)\n");

        auto stats = f.stats();
        std::printf("blocos lidos nesta reabertura: %llu\n",
                    static_cast<unsigned long long>(stats.reads));
        f.close();
    }

    std::printf("\nintegracao (hash_bucket) OK: cabecalho, slots e "
                "encadeamento de overflow funcionando corretamente.\n");
    return 0;
}
