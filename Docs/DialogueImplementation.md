# Diálogos — etapa 4c

## Comportamento

Interagir com um NPC inicia falas sequenciais, sem escolhas de resposta. O mundo continua ativo: a conversa não oferece proteção contra eventos externos. Movimento, câmera manual, agachamento, inventário e outras interações ficam bloqueados enquanto a apresentação está aberta. A câmera aponta para o componente LookTarget do NPC e acompanha sua posição por um timer ativo somente durante a conversa.

A ação Interagir avança uma fala e confirma a conclusão após a última. A ação Pausa cancela sem concluir. No mapeamento atual, são E e Esc. Não foram adicionadas teclas físicas paralelas ao Enhanced Input; remapear essas ações também altera o controle da conversa. A apresentação usa nomes de ações em sua instrução, sem prometer uma tecla fixa após remapeamento.

Cancelar reinicia a conversa na primeira fala na próxima interação. A repetição após conclusão é configurável por diálogo. O exemplo permite repetição, mas só a primeira conclusão registra seu fato de progresso. Ao sair, o jogador permanece olhando na direção do NPC e recupera seus controles.

## Arquitetura e reutilização

O plugin runtime HorrorDialogue depende apenas de Core, CoreUObject e Engine:

- DialogueDefinition define ID estável, nome do NPC, falas, repetição, condições e fato de conclusão.
- DialogueSessionComponent mantém a sessão atual e o histórico de IDs concluídos. Start, Advance e Cancel são independentes de mundo, UI, save ou NPC concreto.
- OnLineChanged e OnEnded notificam consumidores por eventos; notificações impedem mutações reentrantes.

No módulo do projeto, DialogueNPC implementa o contrato de interação e fornece o ponto de câmera. Seu mesh é configurável e pode ser composto em Blueprint; o NPC de demonstração é um placeholder sem animação ou IA. DialogueGameplayComponent conecta alvo, sessão, apresentação, câmera, objetivos e persistência. DialogueScreen apresenta nome, fala e posição na sequência.

Essa divisão aplica composição por Component e comunicação por Observer, sem acrescentar dependência de diálogos aos plugins de interação, inventário, objetivos ou save. As referências conceituais são [Component](https://gameprogrammingpatterns.com/component.html) e [Observer](https://refactoring.guru/design-patterns/observer).

Para reutilizar o núcleo, copie o plugin, adicione DialogueSessionComponent ao proprietário desejado, carregue a definição e conecte os eventos à apresentação do novo jogo. O adaptador específico deste framework continua no módulo do projeto.

## Integração com UI e input

UIScreenBase ganhou duas políticas com os padrões anteriores preservados: bPauseWorld=true e bUseGameplayInput=false. DialogueScreen usa false/true. A pilha ainda bloqueia o HUD, movimento e olhar; a política decide se o mundo pausa e se a tela permite encaminhar ações ao gameplay. O controller encaminha as ações existentes Interagir/Pausa à tela superior quando ela opta por esse contrato. O modo GameAndUI mantém foco na tela para consumir o comando Voltar padrão, inclusive Esc no Editor; comandos não tratados continuam para o Enhanced Input. Não há binding adicional de E no widget ou pawn.

O fluxo de jogo consulta a política de pausa da pilha; uma conversa aberta não passa a simular um jogo pausado. Menus comuns continuam pausando. RemoveScreen permite encerrar somente a apresentação associada, inclusive se outra tela, como game over, for colocada acima dela.

O scanner é suspenso durante a conversa e reativado somente se estava ativo antes. O foco de interação é limpo. A velocidade e o input de movimento acumulado são descartados ao iniciar, evitando deslizamento causado pelo comando anterior.

## Configuração

Crie um Data Asset DialogueDefinition e configure DialogueId único, SpeakerName e Lines com pelo menos uma fala não vazia. bRepeatable permite ou proíbe reabrir após conclusão. RequiredFacts exige todos os fatos listados; BlockingFacts impede a conversa quando qualquer um existe no journal. Condições contraditórias ou vazias são rejeitadas. CompletionFact é opcional e pode concluir/ativar objetivos.

Coloque DialogueNPC no mapa ou derive um Blueprint. Configure Dialogue, NPCMesh, LookTarget e distância no Interactable. A definição carrega assincronamente; o prompt só aparece quando ela e as condições estão prontas. A interação revalida alcance, câmera, obstrução e disponibilidade no momento de iniciar.

O componente Save do NPC deve ter uma chave persistente única. O personagem composto AInventoryCharacter possui Dialogue e DialogueGameplay. bEnableDialogue, configurado antes de BeginPlay, desativa a integração. ScreenClass permite substituir a apresentação.

## Persistência e interrupções

O histórico persiste somente os IDs concluídos. Fala atual, câmera travada, widget e timers são temporários. Um save durante uma conversa registra o progresso já concluído; carregar cancela a sessão em andamento. Uma conversa interrompida deve começar novamente. Alterar IDs exige migração explícita de conteúdo; não há conversão automática de históricos.

O SaveSystem existente serializa o componente do personagem e a referência da definição no NPC. Carregar não repete eventos de conclusão. A transição de apresentação e a notificação de conclusão impedem capturas intermediárias inconsistentes.

Destruição/remoção do NPC, troca de controller/pawn, carregamento, fim da sessão ou substituição por outra tela encerram a apresentação. O timer também verifica game over e transição de mapa. A liberação de controles usa a propriedade de posse da UI existente; não zera bloqueios pertencentes a outros sistemas.

## Validação

Abra `/Game/Maps/FrameworkDialogue_Demo`. Os assets próprios ficam em `/Game/_Project/Dialogue`: BP_DialoguePlayer, BP_DialogueGameMode, DA_CaretakerIntro, DA_TalkCaretaker e DA_DialogueReadMemo. O NPC à frente do ponto inicial usa um cilindro como placeholder. A conversa tem três falas e permite repetição; o registro está atrás dele, à lateral do corredor. Concluir a conversa libera o objetivo de leitura, e a leitura libera o objetivo dos fusíveis. Os encaixes apontam para a porta da cópia do mapa.

Os resultados executados e as limitações estão em [Validation.md](Validation.md). Os testes usam snapshots em memória ou um slot com prefixo HorrorFramework_Dialogue_Test_. O teste em dois processos recusa sobrescrever um arquivo existente e remove apenas seu arquivo exclusivo após a leitura aprovada. Esta etapa não inclui ramificações, dublagem, animações de fala ou IA de NPC.
