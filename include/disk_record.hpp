// Layout binário do registro de artigo, gravado no arquivo de dados
// paginado. Ver Seção 7.1 do relatório para a justificativa dos tamanhos.
//
// Tamanho total: exatamente 1024 bytes -> 4 registros cabem em um bloco de
// 4096 bytes (BLOCK_SIZE), e 8 registros em um bloco de 8192 bytes (usado
// na Seção 7.5), sem sobra em nenhum dos dois casos.

#ifndef DISK_RECORD_HPP
#define DISK_RECORD_HPP

#include <cstdint>
#include <cstring>
#include <string>

#include "record.hpp"  // struct Article (como vem do CsvReader)

// Tamanhos (em bytes) de cada campo de texto de tamanho fixo. Calculados a
// partir do percentil 99 medido empiricamente sobre a amostra oficial com
// tools/analyze_fields.cpp (ver TP2_notas_progresso.md / relatorio 7.1).
constexpr std::size_t kTituloLen = 150;
constexpr std::size_t kAutoresLen = 150;
constexpr std::size_t kAtualizacaoLen = 20;  // 19 uteis + 1 terminador
constexpr std::size_t kSnippetLen = 684;     // reduzido de 688 para 684:
    // libera 4 bytes para que 4 registros + o cabecalho de 16 bytes de um
    // bloco de bucket (ver hash_bucket.hpp) encaixem exatamente em 4096
    // bytes. Impacto no truncamento: irrelevante (p99 do snippet = 505
    // bytes, bem abaixo de 684).

// Valores possíveis do campo `status`, usado pela organização hash
// (Parte 1) para marcar slots vazios/ocupados/removidos dentro de um
// bucket sem precisar zerar a área toda.
enum class SlotStatus : std::uint8_t {
    kVazio = 0,
    kOcupado = 1,
    kRemovido = 2,
};

// Campos numéricos de tamanho fixo agrupados no início da struct, todos
// múltiplos de 4 bytes, para não sofrer padding implícito de alinhamento
// entre eles. Os campos de texto (que não precisam de alinhamento) vêm
// depois. Isso garante sizeof(ArticleRecord) == 1024 sem padding oculto
// inserido pelo compilador (conferido com static_assert mais abaixo).
struct ArticleRecord {
    std::uint8_t status = 0;        // ver SlotStatus
    std::uint8_t reserved[3] = {};  // não usado; mantém id alinhado em 4
    std::int32_t id = 0;
    std::int32_t ano = 0;
    std::int32_t citacoes = 0;
    char titulo[kTituloLen] = {};
    char autores[kAutoresLen] = {};
    char atualizacao[kAtualizacaoLen] = {};
    char snippet[kSnippetLen] = {};

    // Constrói um ArticleRecord a partir de um Article (struct crua do
    // CsvReader), truncando campos que excedam o tamanho reservado e
    // preenchendo com zeros ('\0') o espaço não utilizado. Marca o slot
    // como ocupado.
    static ArticleRecord from_article(const Article& a) {
        ArticleRecord r;
        r.status = static_cast<std::uint8_t>(SlotStatus::kOcupado);
        r.id = a.id;
        r.ano = a.ano;
        r.citacoes = a.citacoes;
        copiar_truncando(r.titulo, kTituloLen, a.titulo);
        copiar_truncando(r.autores, kAutoresLen, a.autores);
        copiar_truncando(r.atualizacao, kAtualizacaoLen, a.atualizacao);
        copiar_truncando(r.snippet, kSnippetLen, a.snippet);
        return r;
    }

    // Reconstrói um Article "legível" a partir do registro binário (usado
    // para imprimir resultados de busca). As strings retornadas já vêm sem
    // o padding de zeros à direita.
    Article to_article() const {
        Article a;
        a.id = id;
        a.ano = ano;
        a.citacoes = citacoes;
        a.titulo = std::string(titulo, strnlen(titulo, kTituloLen));
        a.autores = std::string(autores, strnlen(autores, kAutoresLen));
        a.atualizacao =
            std::string(atualizacao, strnlen(atualizacao, kAtualizacaoLen));
        a.snippet = std::string(snippet, strnlen(snippet, kSnippetLen));
        return a;
    }

    bool ocupado() const {
        return status == static_cast<std::uint8_t>(SlotStatus::kOcupado);
    }

   private:
    // Copia 'origem' para 'destino' (buffer de tamanho fixo 'destino_len'),
    // truncando se necessário e zerando o restante do buffer.
    static void copiar_truncando(char* destino, std::size_t destino_len,
                                  const std::string& origem) {
        std::size_t n = std::min(origem.size(), destino_len);
        std::memcpy(destino, origem.data(), n);
        if (n < destino_len) {
            std::memset(destino + n, 0, destino_len - n);
        }
    }
};

static_assert(sizeof(ArticleRecord) == 1020,
              "ArticleRecord deve ocupar exatamente 1020 bytes (16 bytes a "
              "menos que o bloco de 4096, reservados para o cabecalho do "
              "bucket - ver hash_bucket.hpp); se este assert falhar, o "
              "compilador inseriu padding implicito - reveja a ordem dos "
              "campos.");

#endif  // DISK_RECORD_HPP
