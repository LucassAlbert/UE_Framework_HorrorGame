# Framework de terror em primeira pessoa

## Requisitos acordados

- Referências de mecânicas: Granny (perseguição, portas e puzzles), Evil Nun (agachar, esconderijos e áreas desbloqueáveis) e Tormented Souls (inspeção, documentos e conversas).
- Ciclo: explorar, coletar, inspecionar, resolver puzzle, fugir, esconder-se e liberar áreas.
- Personagem/controller padrão compartilhado pelos futuros jogos.
- Sistemas reutilizáveis em outros projetos, com dependências explícitas. Desativar uma funcionalidade não exige remover fisicamente seu plugin.
- Núcleo obrigatório: controle, save, UI, áudio, boot e gerenciamento de jogo.
- Configuração por Data Assets; C++ para contratos e sistemas, Blueprints para composição e conteúdo simples.
- Interação inicial por uma tecla; esconder prompts de ações indisponíveis.
- Inventário com slots limitados, ações de usar/equipar/combinar/consumir/descartar/dropar. Sem estados individuais diferentes para itens iguais inicialmente.
- Capacidade inicial: cinco slots; limite de stack por item. Combinações consomem uma unidade de cada ingrediente e criam o resultado. Descartar começa desativado.
- Chave usada pelo inventário em uma porta próxima, consumida após destrancar. Uso/equipamento bem-sucedidos fecham a tela. Drop de uma unidade com física.
- Inspeção independente, acessada por um item coletado, com rotação/zoom e mundo pausado.
- Equipamentos iniciais: lanterna na mão esquerda e chave.
- Save manual em pontos específicos, cinco slots, restauração completa após morte.
- Ambientes conectados; persistência de posição, inventário, equipamento, portas, puzzles, coleta, inimigos e eventos narrativos.
- HUD separado das telas modais. Telas modais pausam e retornam à anterior ao fechar.
- Placeholders para menu, pausa, configurações, inventário, inspeção, documentos, objetivos, seleção de saves e game over.
- Teclado/mouse e controle; opções de remapeamento, sensibilidade, FOV, legendas e efeitos de câmera conforme os recursos implementados.
- Avaliar integração de SimpleSettingsMenu sem assumir que seus assets já atendem aos contratos novos.
- Áudio com volumes separados para ambiente, SFX, UI e música.
- IA com patrulha e investigação; esconderijos com olhar limitado e entrada/saída animadas.
- Puzzles, objetivos, documentos, diálogos e sustos.

## Ordem de implementação

1. Consolidar save, pilha de UI, validação de interação, input e boot existentes.
2. Inventário por componente e definições de itens em Data Assets; pickup/drop e equipamento.
3. Inspeção independente, rotação/zoom e integração com inventário/UI.
4. Puzzles, portas/áreas, documentos, objetivos e diálogos configuráveis.
5. Áudio centralizado e configurações persistentes.
6. IA, perseguição/investigação e esconderijos com apresentação animada.
7. Completar restauração do mundo, criação/destruição de atores e transições de mapa.
8. Integrar e validar o cenário completo de demonstração, incluindo cook/build de runtime.

## APIs da primeira etapa

### SaveSystem

`SaveToSlot(0..4)` e `LoadFromSlot(0..4)` iniciam uma operação assíncrona. O retorno indica se ela foi iniciada. O resultado final vem de `OnSaveCompleted` e `OnLoadCompleted`. `SaveGame`/`LoadGame` continuam como atalhos para o slot zero.

`USaveComponent` torna o ator participante do save; a interface é opcional e serve para os callbacks. Propriedades persistentes de atores e componentes devem ter `SaveGame`. Para atores criados em runtime, configure `PersistentKey` ou um GUID persistente antes de `BeginPlay`; não use nomes de spawn como identidade. O personagem padrão usa a chave `Player`.

A etapa 2 acrescenta reconstrução opt-in de atores runtime e remoção de drops criados depois do snapshot, com classes/dependências carregadas assincronamente e persistência de velocidades físicas. Atores colocados continuam exigindo presença/identidade estável. A versão 2 lê snapshots da versão 1. Load entre mapas, streaming e migrações de gameplay ainda precisam ser implementados. GUIDs ou chaves estáveis de atores colocados devem ser persistidos antes de empacotar.

### HorrorUI

`OpenScreen` abre uma tela modal. `PopScreen`/`CloseScreen` retornam à anterior. `OpenGameplayUI` cria um HUD separado, sem duplicá-lo ao retomar o jogo. `ClearScreens` encerra telas na transição de mundo.

O Data Asset existente ganhou classes opcionais para as novas telas. `bEnablePlaceholderScreens` habilita explicitamente as telas nativas de protótipo para classes ainda não configuradas. Os placeholders não implementam inventário, inspeção ou seleção funcional de saves.

### Interação e boot

O scanner usa timer configurável e o canal de interação do projeto. A execução consulta `CanInteract`, distância, o trace atual e obstruções. Instale o scanner no personagem ou controller. O boot aguarda a configuração de UI e o controller do mundo antes de abrir a apresentação adequada.

## Critério final do mapa de demonstração

A etapa 2 está documentada em [InventoryImplementation.md](InventoryImplementation.md). O mapa separado `FrameworkInventory_Demo` exercita itens, combinação, lanterna e porta. A etapa 3 conecta o evento do inventário ao plugin independente de inspeção, com rotação livre, zoom e retorno à tela anterior; veja [InspectionImplementation.md](InspectionImplementation.md) e os resultados de validação em `Validation.md`.

A etapa 4a implementa puzzles de encaixe, portas/áreas e objetivos; veja [ProgressionImplementation.md](ProgressionImplementation.md). A etapa 4b adiciona documentos em coleção própria, leitura paginada com imagem opcional, objetivos e persistência; veja [DocumentsImplementation.md](DocumentsImplementation.md). A etapa 4c acrescenta diálogos sequenciais com NPCs, mundo ativo, câmera travada e histórico persistente; veja [DialogueImplementation.md](DialogueImplementation.md). Cada etapa tem seu mapa separado. Áudio centralizado e configurações são a próxima etapa planejada, sujeita à definição de escopo com o usuário.

Iniciar pelo menu; explorar; coletar um item; inspecioná-lo; resolver um puzzle e liberar uma área; observar patrulha/investigação; fugir e esconder-se; ler documento e acompanhar objetivo/diálogo; salvar em um ponto; modificar o mundo e restaurar o estado salvo. Repetir a restauração após morte e após fechar/reabrir o jogo.

A consolidação posterior à etapa 4c está descrita em [FrameworkConsolidation.md](FrameworkConsolidation.md): propriedade de pausa/bloqueios, troca de pawn, limpeza após restauração, mapeamentos de controle e distribuição Windows.

A etapa de áudio e volumes foi implementada, descrita em [AudioImplementation.md](AudioImplementation.md): pausa por definição, prévia imediata, Aplicar para gravar e restauração dos últimos valores aplicados ao cancelar. Inclui integração do menu, assets e FrameworkAudio_Demo. Build completo, suíte automatizada, reinício das preferências e pacote Windows passaram. O usuário confirmou o funcionamento dos exemplos sonoros e do cancelamento na nova tela; as verificações adicionais pendentes estão em Validation.md. As demais configurações gerais não foram ampliadas nesta etapa.

A primeira etapa não equivale à conclusão desse cenário. Build, testes automatizados, PIE e cook devem ser registrados separadamente, com os resultados realmente executados.

A etapa 6 foi implementada, descrita em [ThreatImplementation.md](ThreatImplementation.md): patrulha, visão/audição, investigação, perseguição, busca e esconderijos; captura imediata com Game Over e último save do mapa atual. Andar emite ruído e agachar é silencioso. O inimigo pode capturar em um esconderijo cuja entrada presenciou. Builds completos, testes e pacote Windows passaram na pasta anterior. O usuário confirmou o retorno ao esconderijo pelo último save e a permanência do Game Over ao testar Esc. O mapa separado é FrameworkThreat_Demo; resultados e limites de validação estão em Validation.md. Animações e carregamento entre mapas ficam para etapas posteriores.
