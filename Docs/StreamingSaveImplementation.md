# Streaming de áreas e seleção de saves — etapa 7

Implementação em andamento. Este documento registra o escopo aceito e a arquitetura planejada; não certifica builds ou testes ainda não executados.

## Decisões confirmadas

- Cinco slots com seleção funcional; Continuar e Game Over usam o save manual mais recente, independentemente do mapa.
- Streaming real de áreas, sem tela de carregamento durante a passagem entre elas.
- Áreas distantes descarregam e preservam seus estados. Não há simulação do tempo transcorrido enquanto uma área está descarregada.
- Inimigos podem atravessar passagens e manter a perseguição. Áreas necessárias a uma perseguição/caminho ativo permanecem residentes até poderem ser descarregadas com segurança.
- Demonstração separada com duas áreas simples e corredor; demonstrações anteriores preservadas.

## Limites e responsabilidades

SaveSystem continua responsável por identidade, snapshots, cache dos estados de áreas, leitura/gravação e reconstrução. O formato novo deve ler as versões 1 e 2. Um contrato de preparação do mundo permite que o carregamento espere os participantes corretos, sem depender do plugin de streaming.

HorrorWorld será o módulo de streaming reutilizável: definição de mundo/áreas em Data Asset, estados explícitos de carregamento, pré-carregamento por proximidade, histerese para descarregar e retenção por atores ativos. A persistência de uma área ocorre antes de seu descarregamento; falhas preservam a área carregada e são diagnosticadas. A restauração precede a liberação de uma área ao gameplay.

Atores móveis que atravessam áreas serão criados no nível persistente por spawners com identidades estáveis. Sua associação de área acompanha a posição; descarregar uma área não deve eliminar um perseguidor ativo. Drops e inimigos descarregados terão estado capturado antes da remoção, sem registrar a remoção de streaming como destruição definitiva.

O módulo do projeto fará a composição da UI, do mundo de demonstração e do inimigo. O fluxo central aguardará o mundo preparado antes de liberar gameplay. Carregar um save pelo menu pode exigir a abertura inicial de seu mapa persistente; a continuidade sem troca completa se aplica às áreas dentro desse mundo. Não se promete ausência de qualquer oscilação de frame por I/O: o pré-carregamento e as passagens precisam impedir acesso a geometria ainda indisponível.

## Situação em 20/10/2026

Código preparado no repositório `UE_Framework_HorrorGame`: plugin HorrorWorld, cache/restauração de áreas no SaveSystem, catálogo dos slots, telas nativas, ponto de save, retenção do perseguidor e coordenação do boot. O formato 3 preserva a leitura dos formatos 1 e 2.

Verificação executada: Unreal Header Tool e compilação C++ do target HorrorGameFrameworkEditor / Development / Win64 na Unreal 5.8.3 com `-NoLink`: **Succeeded**. Esse modo não vincula nem substitui as DLLs do Editor aberto e não equivale ao build completo exigido para entrega. Foram corrigidos os conflitos de nomes refletidos, os handles de FTSTicker e o consumidor de teste do carregamento assíncrono.

Pendências: fechar o Editor normalmente após salvar alterações do usuário; executar build completo; criar/configurar os assets e os três mapas da demonstração; compilar os Blueprints consumidores; executar os novos testes e a regressão; validar travessia, perseguição, cinco slots, restauração entre mapas e reinício; executar cook. Nenhum desses testes de gameplay/cook foi certificado nesta etapa ainda. Os assets das demonstrações anteriores não foram modificados.

Os novos testes de código cobrem seleção com histerese, retenção de rotas, validação do grafo de áreas, cache de atores removidos pelo streaming, preservação de outras áreas e metadados legados. Estão compilados, mas ainda não foram executados.

Ao carregar um arquivo no mesmo mundo, as áreas são reconstruídas a partir dos padrões autorais antes de aplicar o snapshot. Isso evita conservar modificações posteriores ao save em uma área que ele ainda não havia visitado.

## Verificações previstas

- Build Editor/runtime na nova pasta com Unreal 5.8.3.
- Catálogo dos cinco slots, arquivos ausentes/inválidos, seleção por data e compatibilidade das versões anteriores.
- Cache/restauração de áreas e remoção de atores runtime sem tombstones indevidos.
- Ida/volta entre duas áreas; pré-carregamento, descarregamento e persistência de pickups, porta/puzzle e drops.
- Perseguição atravessando áreas, save/load com esconderijo e carregamento pelo menu/Game Over.
- Interrupções de carregamento, limpeza de delegates/timers, retorno do input e cook/pacote.

## Referências

- https://dev.epicgames.com/documentation/en-us/unreal-engine/level-streaming-overview-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/level-streaming-using-volumes-in-unreal-engine
- https://refactoring.guru/design-patterns/observer
- https://gameprogrammingpatterns.com/state.html
