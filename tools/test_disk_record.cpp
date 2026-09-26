#include <cassert>
#include <cstdio>

#include "disk_record.hpp"

int main() {
    std::printf("sizeof(ArticleRecord) = %zu bytes\n", sizeof(ArticleRecord));

    // Caso normal: campos curtos, cabem sem truncar.
    Article a1;
    a1.id = 42;
    a1.titulo = "Um titulo qualquer";
    a1.ano = 2020;
    a1.autores = "Fulano|Beltrano";
    a1.citacoes = 7;
    a1.atualizacao = "2020-01-01 00:00:00";
    a1.snippet = "um snippet curto";

    ArticleRecord r1 = ArticleRecord::from_article(a1);
    assert(r1.ocupado());
    Article back1 = r1.to_article();
    assert(back1.id == a1.id);
    assert(back1.titulo == a1.titulo);
    assert(back1.autores == a1.autores);
    assert(back1.ano == a1.ano);
    assert(back1.citacoes == a1.citacoes);
    assert(back1.atualizacao == a1.atualizacao);
    assert(back1.snippet == a1.snippet);
    std::printf("caso normal: OK (ida e volta preserva os dados)\n");

    // Caso de truncamento: titulo maior que kTituloLen (150 bytes).
    Article a2;
    a2.id = 99;
    a2.titulo = std::string(200, 'X');  // 200 bytes, maior que 150
    a2.autores = "Y";
    a2.atualizacao = "2021-01-01 00:00:00";
    a2.snippet = "s";

    ArticleRecord r2 = ArticleRecord::from_article(a2);
    Article back2 = r2.to_article();
    assert(back2.titulo.size() == kTituloLen);              // truncado
    assert(back2.titulo == std::string(kTituloLen, 'X'));   // conteudo certo
    std::printf("caso de truncamento: OK (titulo cortado em %zu bytes)\n",
                kTituloLen);

    // Slot vazio (status default) deve reportar ocupado() == false.
    ArticleRecord vazio;
    assert(!vazio.ocupado());
    std::printf("slot vazio: OK (ocupado() == false)\n");

    std::printf("\ntodos os testes passaram.\n");
    return 0;
}
