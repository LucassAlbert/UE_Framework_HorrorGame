# AGENTS.md — Horror Game Framework

## Contexto

Este repositório contém um jogo/framework de terror em primeira pessoa em Unreal Engine, com C++ e Blueprints. Priorize estabilidade, legibilidade, desempenho e consistência de interação, exploração e tensão.

- Projeto: `HorrorGameFramework/HorrorGameFramework.uproject`.
- Associação atual: Unreal 5.8. Versão validada neste ambiente: 5.8.3. Confirme a instalação antes de executar comandos; não presuma caminhos de outras máquinas.
- Plataforma configurada: Windows. Não introduza multiplayer, serviços online ou novas plataformas sem necessidade da tarefa.
- Responda em português brasileiro. Use inglês em identificadores de código e siga as convenções existentes para comentários e assets.
- Estas instruções abrangem o repositório inteiro. Leia instruções em subpastas antes de trabalhar nelas. Instruções explícitas do usuário prevalecem.

## Forma de trabalhar

1. Leia a implementação, seus consumidores e o estado do Git antes de editar. Identifique o módulo responsável e preserve alterações existentes.
2. Faça a menor alteração coerente que resolva a causa do problema. Não esconda falhas desativando funcionalidades ou introduzindo fallbacks silenciosos.
3. Prossiga autonomamente em trabalho autorizado e reversível. Peça esclarecimento apenas quando uma decisão material não puder ser inferida; continue trabalho independente enquanto aguarda.
4. Não reverta arquivos, exclua conteúdo, troque a engine, faça commit ou push sem autorização correspondente. Não amplie a tarefa com refatorações ou mudanças de design não solicitadas.
5. Valide de acordo com o risco. Informe o comportamento alterado, verificações realmente executadas e limitações. Nunca declare sucesso de um teste que não foi executado.

## Arquitetura existente

Respeite os limites dos módulos, sem reorganizar o framework como efeito colateral:

| Local | Responsabilidade |
| --- | --- |
| `Plugins/HorrorCore` | Personagens, controller, game mode, game instance, boot e fluxo do jogo |
| `Plugins/HorrorInteraction` | Detecção, interfaces, componentes de interação e esconderijos |
| `Plugins/HorrorUI` | Telas, configurações e gerenciamento da interface |
| `Plugins/SaveSystem` | Persistência, registro de atores e componentes de save |
| `Source/HorrorGameFramework` | Integração e código específico do projeto |
| `Plugins/AsyncLoadingScreen` | Dependência de terceiros; preservar licença e origem em `UPSTREAM.txt` |

Os caminhos da tabela são relativos a `HorrorGameFramework/`.

Use componentes para capacidades reutilizáveis, interfaces para contratos e subsystems quando o ciclo de vida corresponder ao sistema. Evite classes que concentrem toda a lógica, cadeias de casts e dependências circulares. Não crie abstrações ou plugins para operações triviais.

Declare dependências em `Build.cs`: públicas quando exigidas por headers públicos, privadas para implementação. Declare também dependências entre plugins nos descritores quando aplicável. Separe ferramentas de editor do runtime; módulos de editor não devem ser exigidos pelo jogo empacotado.

## C++ e ciclo de vida do Unreal

- Siga a nomenclatura Unreal: `U`, `A`, `F`, `E`, `I` e `b` para booleanos. Prefira funções pequenas, nomes claros e responsabilidade única.
- Inclua diretamente os headers usados, sem depender de includes transitivos, PCH ou unity build. O `.generated.h` deve ser o último include do header refletido.
- Use `const`, referências e forward declarations quando apropriado. Evite cópias grandes, valores mágicos e caminhos absolutos de máquina no jogo.
- Para referências fortes refletidas a `UObject`, use `UPROPERTY` e `TObjectPtr` conforme o contexto. Use referências fracas para relações não proprietárias e soft references para assets carregados sob demanda.
- Nunca crie/destrua `UObject` com `new`/`delete`. Use `NewObject`, `CreateDefaultSubobject` ou `SpawnActor` conforme o contexto.
- Não acesse o mundo nem execute gameplay no construtor; preserve o comportamento do CDO. Configure componentes no construtor e inicialize comportamento no callback apropriado.
- Chame `Super` quando o contrato exigir. Libere timers, delegates e tarefas pendentes em `EndPlay`, `Deinitialize` ou no encerramento correspondente. Evite binds duplicados e callbacks para objetos destruídos.
- Use guard clauses para falhas esperadas, `ensure` para condições inesperadas recuperáveis e `check` para invariantes. Não trate arquivos ausentes ou ações inválidas do jogador com asserts fatais.
- Em operações assíncronas, respeite o ciclo de vida dos objetos e a thread exigida pela API. Faça trabalho pesado fora da game thread apenas quando seguro; não manipule o mundo arbitrariamente em workers.
- Exponha apenas o necessário a Blueprints, com categorias e metadados claros. Preserve APIs refletidas usadas por assets; quando renomear classes/propriedades, avalie Core Redirects e migração.
- Preserve a codificação dos arquivos existentes e use UTF-8 em arquivos novos. Comentários devem explicar intenção e decisões, sem repetir o código.

## C++ e Blueprints

Use C++ para contratos, sistemas reutilizáveis e lógica que exija controle ou desempenho. Use Blueprints para composição, configuração, apresentação e eventos de conteúdo. Não migre código funcional apenas por preferência pessoal.

- Organize gráficos em funções curtas, categorias e comentários úteis.
- Evite lógica essencial espalhada por Level Blueprints, polling em Event Tick e casts repetidos.
- Prefira eventos, delegates e interfaces para comunicação. Use propriedades editáveis, Data Assets e tabelas quando ajudarem o designer a configurar o comportamento.
- Após alterar APIs refletidas, compile os Blueprints consumidores e confira suas referências. Não salve alterações acidentais de assets fora da tarefa.

## Terror em primeira pessoa

- Use o Enhanced Input existente. Gerencie contextos de gameplay/UI e bloqueios por pausa, morte, cutscene ou perda de foco. Não duplique o mesmo input no pawn, controller e UI.
- Separe responsabilidades de câmera, movimento e interação. Faça movimento, durações e consumo de recursos independentes da taxa de frames.
- Valide interação no momento da execução: alvo, distância, visibilidade, estado e permissões. Considere alvo destruído, porta bloqueada e troca rápida de foco.
- Configure canais/filtros de traces e frequência de detecção conforme a necessidade. Atualize prompts quando o alvo ou estado mudar, sem varrer todos os atores por frame.
- Modele estados do jogador, inimigos, puzzles e fluxo explicitamente. Evite booleanos contraditórios; garanta reset após morte, transição de mapa e carregamento.
- Reutilize a solução de IA existente. Considere percepção, navegação, investigação, busca e retorno ao estado normal, sem impor uma tecnologia nova sem justificativa.
- Eventos de susto/narrativa precisam de condições, cooldowns e regras de repetição. Considere checkpoints e evite bloqueios permanentes de progresso.
- Use áudio espacial e sinais visuais coerentes. Limpe loops, widgets e efeitos ao sair do estado ou destruir o ator.
- Não faça pistas obrigatórias dependerem exclusivamente de som ou de um efeito visual. Em tarefas de câmera/opções, considere sensibilidade, FOV, camera shake e head bob ajustáveis conforme o design existente.
- Teste interrupções, transições e condições de baixa taxa de frames, além do caminho esperado.

## Desempenho

- Prefira eventos/timers a Tick quando não for necessária atualização contínua; habilite Tick somente enquanto útil.
- Evite `GetAllActorsOfClass`, carregamentos síncronos, alocações grandes e reconstrução de widgets em caminhos frequentes sem necessidade medida.
- Atualize UI por eventos quando possível; não coloque lógica pesada em bindings recorrentes.
- Evite referências hard que carreguem cadeias grandes de assets sem necessidade. Planeje carregamento assíncrono e estados de espera para recursos pesados.
- Use pooling somente quando criação/destruição frequente for um custo real e houver regras claras de reset.
- Investigue gargalos com Unreal Insights e estatísticas da engine. Registre cenário e resultado antes de otimizações amplas; PIE sozinho não representa desempenho de um build empacotado.

## Saves e checkpoints

- Respeite o `SaveSystem` existente. Persista estado necessário à reconstrução do jogo, sem ponteiros brutos ou referências temporárias do mundo.
- Identifique atores persistentes de forma estável; não dependa apenas de nomes gerados no spawn.
- Considere versionamento/migração, arquivos ausentes ou corrompidos e valores inválidos. Preserve compatibilidade de saves quando viável.
- Evite gravações pesadas na game thread e saves concorrentes. Confirme sucesso antes de anunciar que um checkpoint foi salvo.
- Verifique restauração após morte, troca de mapa e reinício: inventário, puzzles, portas e eventos consumidos, conforme a tarefa.

## Assets, Git e MCP

- Não edite `.uasset`/`.umap` como texto. Use Unreal Editor ou ferramentas MCP apropriadas e confira referências antes de mover, renomear ou excluir assets.
- Antes de ações MCP, descubra toolsets, ferramentas e schemas. Não invente argumentos, nomes de propriedades ou caminhos de objetos.
- Confirme projeto/nível ativos antes de alterações de cena. Respeite PIE e alterações não salvas do usuário; não encerre sessões ou o editor sem necessidade.
- Após modificar pelo MCP, confira o resultado e salve somente os assets envolvidos. Consulte o Output Log para diagnosticar falhas.
- Siga os prefixos existentes; exemplos usuais são `BP_`, `WBP_`, `DA_`, `DT_`, `M_`, `MI_`, `T_` e `S_`. Não renomeie conteúdo apenas para padronizar.
- Preserve Git LFS para binários. Um ponteiro LFS sem conteúdo local deve ser resolvido antes de diagnosticar corrupção do asset.
- Não edite manualmente `Binaries`, `Intermediate`, `Saved` ou `DerivedDataCache`. Ferramentas podem regenerá-los durante validação; não faça limpezas amplas nem inclua saídas geradas no escopo de commits de código.
- Não remova plugins habilitados para contornar falhas sem verificar seus consumidores. Prefira restaurar dependências compatíveis e documente qualquer perda de funcionalidade necessária.

## Validação e entrega

Escolha verificações proporcionais à alteração:

- **C++/módulos/plugins:** compile `HorrorGameFrameworkEditor` em `Development / Win64` com a engine associada. Live Coding não substitui build completo para mudanças em headers refletidos, targets ou descritores.
- **Gameplay:** reproduza em PIE o cenário alterado, interrupções e retorno ao estado normal. Para input/câmera/interação, verifique pausa/menu e morte ou troca de pawn quando aplicável.
- **Blueprints/assets:** compile os consumidores afetados, confira referências, abra o nível pertinente e examine o Output Log.
- **Saves:** verifique gravação e restauração do estado modificado.
- **Runtime/build:** faça cook ou empacotamento quando a mudança afetar dependências, inclusão/carregamento de assets ou comportamento fora do editor.
- Crie testes automatizados para lógica relevante, bugs recorrentes e invariantes difíceis de verificar manualmente. Evite testes triviais que apenas reproduzam a implementação.
- Se uma verificação estiver indisponível, descreva o bloqueio concreto e o que falta testar. Diferencie erros bloqueantes de avisos existentes.

Use categorias de log por sistema, mensagens acionáveis e níveis adequados. Evite spam por frame e debug visual no build final. Na entrega, explique o efeito para o jogador, cite os arquivos relevantes e relate exatamente as verificações realizadas e os riscos restantes.
