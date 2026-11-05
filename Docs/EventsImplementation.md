# Eventos e sustos

Implementação de 05/11/2026 para Unreal 5.8.3 / Windows. O cenário está em `/Game/Maps/FrameworkIntegrated_Demo`. Os resultados executados ficam em [Validation.md](Validation.md).

## Comportamento acordado

O evento da demonstração é disparado ao entrar no corredor, uma vez por instância da partida. Uma lâmpada pisca três vezes, ocorre um ruído próximo e uma porta lateral bate. Um fato de progresso registra a conclusão. O personagem continua livre para se mover durante a sequência. Os timers acompanham a pausa do mundo, e os sons respeitam suas definições do sistema de áudio.

Um snapshot capturado durante a sequência restaura seu estado final e a considera concluída. Essa restauração aplica os estados duráveis e registra o fato, sem reproduzir os sons. Um evento já concluído mantém o consumo salvo. Carregar um snapshot anterior ao susto restaura o evento como não consumido. Reiniciar a partida cria novamente seus estados iniciais.

## Organização

| Elemento | Responsabilidade |
| --- | --- |
| HorrorEvents | Plugin runtime com dependências somente de Core, CoreUObject e Engine. |
| UHorrorEventDefinition | ID, revisão, execução única, intervalo entre execuções, condições e sequência de ações com seus atrasos. |
| UHorrorEventCondition | Contrato de condição configurável por objeto instanciado no asset, extensível em C++ ou Blueprint. Recebe explicitamente o possível instigador. |
| UHorrorEventAction | Contratos de pré-validação, execução e restauração final. Os objetos do asset são configurações compartilhadas, e o estado de execução pertence ao componente. |
| UHorrorEventComponent | Prontidão dos recursos, execução ordenada por timers, bloqueio de reentrada, progresso SaveGame e restauração. |
| IHorrorEventTarget / UHorrorEventStateAction | Aplica um estado nomeado em um alvo vinculado pelo mapa. O asset usa um TargetId e o componente associa esse ID ao ator local por referência soft. |
| AFrameworkEventActor | Adaptador do projeto para volume, interação ou alterações de progresso, com SaveComponent e vínculos ao jogador. |
| AFrameworkEventProp | Alvo de apresentação com estados persistentes para visibilidade, iluminação, posição e rotação relativas da malha. |

As condições fornecidas pelo projeto verificam fatos exigidos/proibidos, quantidades de itens e estado de um objetivo. Os fatos existentes de puzzles, documentos e diálogos podem alimentar essas condições. As ações fornecidas reproduzem sons pelo HorrorAudio, registram fatos, ativam/desativam inimigos e aplicam estados nomeados. A ativação de um inimigo que já está ativo preserva seu estado em curso.

Gatilhos de progresso observam os delegates do inventário e do journal. O adaptador acompanha a posse do pawn e troca suas inscrições ao mudar de jogador controlado. A implementação usa eventos e timers, sem polling em Event Tick. Os contratos de notificação e a composição por domínio seguem as referências de [Observer](https://refactoring.guru/design-patterns/observer) e [Component](https://gameprogrammingpatterns.com/component.html).

## Configurar outro evento

1. Crie um HorrorEventDefinition e atribua EventId e Revision. Os IDs identificam a definição usada pelo estado salvo. Incremente Revision quando mudar de forma incompatível a sequência ou seus requisitos.
2. Adicione condições e passos. Cada passo possui DelayBefore e uma ação instanciada no próprio asset. Um atraso zero executa no próximo tick, sem recursão da pilha de chamadas.
3. Coloque AFrameworkEventActor no mapa e selecione a definição no componente Event.
4. Escolha Volume, Interaction ou Progression. Para Volume, ajuste a caixa. Para Interaction, configure o texto e a área atingida pelo canal de interação. Progression avalia as condições quando o journal ou inventário muda.
5. Preencha o mapa Targets com os mesmos IDs usados pelas ações. Os atores precisam estar residentes no momento da execução. O alvo padrão implementa estados nomeados, e Blueprints podem implementar HorrorEventTarget para outros comportamentos.
6. Alvos com estado durável precisam participar do save e restaurar suas próprias propriedades. AFrameworkEventProp já faz isso por CurrentState. Um snapshot concluído usa esses estados salvos, preservando mudanças posteriores ao evento.
7. Para emitir ruído percebido pela IA, configure Noise.Definition no ator e habilite EmitHearingNoise nas ações sonoras desejadas. A percepção usa a definição de ruído e continua independente dos volumes escolhidos pelo jogador.

O componente pode ser desativado por SetEventsEnabled ou pela configuração inicial Enabled. Desativar encerra timers e amostras sonoras da instância. O contador de conclusões preserva o consumo de eventos de execução única. Em eventos repetíveis, o intervalo impede um novo disparo até terminar, e um novo estímulo pode solicitar a próxima execução.

## Estado e persistência

O componente serializa EventId, Revision, State, NextStep, CompletedCount e CooldownRemaining. O intervalo restante é atualizado na serialização SaveGame, sem atualização por frame. Reentradas são recusadas enquanto uma condição, ação ou delegate de execução está sendo despachado.

A integração valida o schema do progresso e, em snapshots Running, os alvos necessários antes da gravação e antes dos callbacks de restauração. Definições inválidas, durações não finitas, revisão incompatível e alvos indisponíveis produzem diagnóstico. As condições são avaliadas no disparo. Após o início, a sequência mantém seu contexto e executa os passos em ordem.

Durante a restauração, as propriedades de todos os participantes são carregadas antes das apresentações. Para Running, RestoreFinalState percorre as ações duráveis em ordem e conclui o evento. Áudio usa a implementação transitória, sem reprodução nessa etapa. Para Completed, o componente mantém o consumo e o intervalo restante, enquanto os próprios alvos restauram o snapshot. Isso evita sobrescrever alterações do mundo feitas após a conclusão.

Uma falha de execução interrompe os timers e sons e registra Failed, sem anunciar conclusão. A sequência não desfaz automaticamente efeitos de passos já executados. Os estados desses alvos continuam sendo os estados reais do mundo, preservados por seus saves. Os callbacks recebem bRestored na conclusão para distinguir conclusão normal e restauração.

O adaptador recusa novos disparos durante pausa, preparação/restauração do mundo e operações de save. No encerramento, remove delegates do jogador e do save, cancela carregamentos e timers e encerra os sons que rastreia.

## Assets da demonstração

- `/Game/_Project/Events/Data/DA_CorridorScare` contém dez passos e execução única.
- `/Game/_Project/Events/Data/DA_CorridorNoise` e `DA_DoorSlam` controlam os sons pelo grupo Effects, com pausa acompanhando o mundo.
- `/Game/_Project/Events/Data/DA_CorridorHearing` configura o estímulo da IA.
- `/Game/_Project/Events/Materials/M_EventLight` apresenta a lâmpada do protótipo.
- Os atores CorridorScareTrigger, CorridorScareLight e CorridorScareDoor ficam no mapa persistente, com chaves de save próprias.
- O fato registrado ao concluir é `Demo.CorridorScare.Completed`.

O gatilho fica aproximadamente em X=-1000 no corredor central, antes da porta do puzzle. A porta lateral do susto é um objeto de apresentação sem colisão. A demonstração usa primitivas de geometria e o som existente SFX_RevolverShot com pitch/volume ajustados como áudio provisório. Os Sound Definitions permitem substituir essas amostras pelas gravações finais.

Snapshots anteriores à inclusão desses atores não possuem seu progresso de evento e não receberam migração de conteúdo nesta etapa. Para testar, inicie uma partida nova e caminhe da área inicial para o corredor central. Observe a lâmpada e a porta lateral, volte e atravesse novamente. Use os pontos de save para verificar os estados anteriores e posteriores. O protocolo automatizado exercita também o snapshot enquanto a sequência está em execução.

## Reutilização

Outro projeto C++ compatível pode copiar e habilitar somente HorrorEvents, criar suas definições, vincular alvos e chamar TryStart. Seus próprios serviços de persistência devem serializar Progress e chamar NotifyStateRestored depois de restaurar os atores. Sons, condições de inventário/progresso, percepção e save deste projeto ficam nos diretórios Public/Events e Private/Events do módulo HorrorGameFramework e exigem os módulos correspondentes ao serem copiados.

A transferência para um projeto vazio, Shipping, gatilhos com alvos em outras áreas descarregadas e uma rodada manual de ritmo do susto continuam como verificações adicionais. Os testes desta etapa usam os alvos persistentes do mapa integrado e processos Windows Development.
