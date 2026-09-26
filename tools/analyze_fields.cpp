// Ferramenta de análise (NÃO faz parte da entrega) — mede o tamanho em
// bytes dos campos de texto do CSV, usando o mesmo CsvReader/Article do
// template, para embasar a decisão de layout do registro (Fase 1 / Seção
// 7.1 do relatório). Também detecta IDs duplicados e registros suspeitos,
// para investigar divergências na contagem total de registros.
//
// Uso: ./analyze_fields <caminho-do-csv>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "csv_reader.hpp"
#include "record.hpp"

namespace {

struct FieldStats {
    std::vector<std::size_t> lens;

    void add(const std::string& s) { lens.push_back(s.size()); }

    void report(const char* name) const {
        if (lens.empty()) {
            std::printf("%-10s: sem dados\n", name);
            return;
        }
        std::vector<std::size_t> sorted = lens;
        std::sort(sorted.begin(), sorted.end());
        std::size_t sum = 0;
        for (auto v : sorted) sum += v;
        double avg = static_cast<double>(sum) / sorted.size();
        auto pct = [&](double p) -> std::size_t {
            std::size_t idx = static_cast<std::size_t>(p * (sorted.size() - 1));
            return sorted[idx];
        };
        std::printf(
            "%-10s: min=%zu  p50=%zu  p90=%zu  p99=%zu  max=%zu  media=%.1f\n",
            name, sorted.front(), pct(0.50), pct(0.90), pct(0.99),
            sorted.back(), avg);
    }

    // Conta quantos registros ultrapassam 'limite' bytes (seriam truncados)
    // e o total de bytes perdidos com o corte em 'limite'.
    void report_truncamento(const char* name, std::size_t limite) const {
        std::size_t afetados = 0;
        std::size_t bytes_perdidos = 0;
        for (auto v : lens) {
            if (v > limite) {
                ++afetados;
                bytes_perdidos += (v - limite);
            }
        }
        double pct_afetados = 100.0 * afetados / lens.size();
        std::printf(
            "%-10s: limite=%-5zu registros truncados=%-7zu (%.3f%%)  "
            "bytes perdidos (total)=%zu\n",
            name, limite, afetados, pct_afetados, bytes_perdidos);
    }
};

// Conta aspas (") por linha física do arquivo, para detectar linhas com
// número ímpar de aspas — sinal de que um campo entre aspas contém uma
// quebra de linha "crua" e continua na próxima linha física. Isso ajuda a
// diagnosticar por que a contagem de registros pode divergir do esperado,
// já que o CsvReader do template processa uma linha física por vez.
void diagnosticar_linhas_partidas(const std::string& path) {
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    std::size_t total_linhas = 0;
    std::size_t linhas_com_aspas_impar = 0;
    while (std::getline(in, line)) {
        ++total_linhas;
        std::size_t n_aspas = std::count(line.begin(), line.end(), '"');
        if (n_aspas % 2 != 0) ++linhas_com_aspas_impar;
    }
    std::printf("\n--- diagnostico de linhas fisicas ---\n");
    std::printf("total de linhas fisicas no arquivo: %zu\n", total_linhas);
    std::printf(
        "linhas com numero impar de aspas (possivel campo com quebra de "
        "linha crua): %zu\n",
        linhas_com_aspas_impar);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "uso: %s <csv>\n", argv[0]);
        return 1;
    }
    const std::string path = argv[1];

    CsvReader reader(path);
    Article a;
    FieldStats titulo, autores, snippet, atualizacao;
    std::size_t total = 0;
    std::map<int, int> id_count;      // id -> quantas vezes apareceu
    std::size_t id_zero = 0;
    int primeiro_id = 0, ultimo_id = 0;

    while (reader.next(a)) {
        ++total;
        titulo.add(a.titulo);
        autores.add(a.autores);
        snippet.add(a.snippet);
        atualizacao.add(a.atualizacao);
        id_count[a.id]++;
        if (a.id == 0) ++id_zero;
        if (total == 1) primeiro_id = a.id;
        ultimo_id = a.id;
    }

    std::printf("registros validos lidos : %zu\n", total);
    std::printf("linhas ignoradas        : %zu\n", reader.skipped());
    std::printf("primeiro id lido        : %d\n", primeiro_id);
    std::printf("ultimo id lido          : %d\n", ultimo_id);
    std::printf("\n--- tamanho dos campos (bytes) ---\n");
    titulo.report("titulo");
    autores.report("autores");
    snippet.report("snippet");
    atualizacao.report("atualizacao");

    diagnosticar_linhas_partidas(path);

    std::printf("\n--- diagnostico de ids ---\n");
    std::printf("ids distintos           : %zu\n", id_count.size());
    std::printf("registros com id == 0   : %zu\n", id_zero);
    std::size_t ids_duplicados = 0;
    std::size_t ocorrencias_extras = 0;
    int impressos = 0;
    for (const auto& [id, count] : id_count) {
        if (count > 1) {
            ++ids_duplicados;
            ocorrencias_extras += (count - 1);
            if (impressos < 20) {
                std::printf("  id=%d aparece %d vezes\n", id, count);
                ++impressos;
            }
        }
    }
    std::printf("ids com duplicata       : %zu\n", ids_duplicados);
    std::printf("ocorrencias extras totais (soma de (count-1)): %zu\n",
                ocorrencias_extras);
    if (ids_duplicados > 20) {
        std::printf("  (mostrando so os 20 primeiros ids duplicados)\n");
    }

    std::printf("\n--- simulacao de truncamento (layout proposto: 1024 bytes/registro) ---\n");
    titulo.report_truncamento("titulo", 150);
    autores.report_truncamento("autores", 150);
    snippet.report_truncamento("snippet", 688);

    return 0;
}
