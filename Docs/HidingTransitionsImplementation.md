# TransiÃ§Ãµes dos esconderijos

## Comportamento aprovado

O jogador interage com o esconderijo usando o comando de interaÃ§Ã£o configurado. A porta abre, o personagem e a cÃ¢mera se deslocam suavemente atÃ© a posiÃ§Ã£o interna, e a porta fecha. O mesmo comando inicia a saÃ­da, que retorna Ã  posiÃ§Ã£o e Ã  orientaÃ§Ã£o de cÃ¢mera anteriores Ã  entrada.

| Estado | Ocupa o esconderijo | ProteÃ§Ã£o da visÃ£o comum | Movimento e olhar livres |
| --- | --- | --- | --- |
| Outside | NÃ£o | NÃ£o | Sim |
| Entering | Sim | NÃ£o | NÃ£o |
| Hidden | Sim | Sim, exceto entrada testemunhada | Somente olhar limitado |
| Exiting | Sim | NÃ£o | NÃ£o |

A pausa interrompe o avanÃ§o da animaÃ§Ã£o. InteraÃ§Ãµes repetidas nÃ£o reiniciam a transiÃ§Ã£o. A saÃ­da Ã© recusada quando a posiÃ§Ã£o de retorno ou o percurso estÃ£o bloqueados. Se surgir um bloqueio durante o deslocamento, a entrada retorna ao ponto seguro quando possÃ­vel; a saÃ­da retorna ao estado escondido. Captura, troca de pawn, destruiÃ§Ã£o do esconderijo e encerramento limpam a animaÃ§Ã£o e a ocupaÃ§Ã£o.

## Responsabilidades

- **HorrorInteraction / UHidingParticipantComponent:** estados explÃ­citos, deslocamento, rotaÃ§Ã£o de cÃ¢mera, colisÃ£o, ocupaÃ§Ã£o e restauraÃ§Ã£o imediata. Tick somente durante Entering/Exiting.
- **UHidingTransitionDefinition:** configuraÃ§Ã£o reutilizÃ¡vel, com duraÃ§Ã£o de entrada/saÃ­da, fraÃ§Ã£o para abrir/fechar a porta, rotaÃ§Ã£o da porta e curva opcional de deslocamento. Sem curva, usa interpolaÃ§Ã£o suave.
- **UHideSpotComponent:** posiÃ§Ã£o interna, distÃ¢ncia, limites do olhar, referÃªncia ao perfil e evento de abertura da porta.
- **HorrorCore / AHorrorPlayerController:** bloqueia os comandos de gameplay durante a transiÃ§Ã£o e preserva a gestÃ£o existente de pausa/UI.
- **HorrorAI:** distingue tentativa de ocultaÃ§Ã£o de ocultaÃ§Ã£o concluÃ­da. A entrada continua visÃ­vel e capturÃ¡vel. A memÃ³ria de entrada testemunhada permanece ao fechar a porta.
- **Projeto / UFrameworkThreatTarget:** adapta estados para percepÃ§Ã£o e save, usando a identidade persistente do esconderijo.
- **Projeto / AFrameworkHidingSpot:** reage ao evento da porta e apresenta a malha. Inclui armÃ¡rio geomÃ©trico opcional para demonstraÃ§Ã£o.

Essa divisÃ£o aplica estados explÃ­citos e comunicaÃ§Ã£o por eventos, seguindo os princÃ­pios de State e Observer sem adicionar hierarquias para cada estado. O plugin de interaÃ§Ã£o permanece independente de save, IA e conteÃºdo do projeto. Os adaptadores conectam esses sistemas no projeto.

## Configurar outro esconderijo

1. Crie um Data Asset da classe HidingTransitionDefinition e escolha os tempos. Zero permite uma transiÃ§Ã£o instantÃ¢nea daquele sentido.
2. Configure TransitionDefinition no HideSpot. A ausÃªncia de perfil preserva o comportamento instantÃ¢neo dos consumidores anteriores.
3. Ajuste HiddenOffset para a posiÃ§Ã£o/orientaÃ§Ã£o do personagem, EntryDistance e os limites de olhar.
4. No FrameworkHidingSpot, configure a malha Door e seu componente DoorPivot na posiÃ§Ã£o da dobradiÃ§a. DoorOpenRotation Ã© relativa Ã  orientaÃ§Ã£o fechada do pivÃ´. Uma tampa pode usar pitch no perfil.
5. Para o protÃ³tipo de armÃ¡rio, habilite UsePrototypeCabinet e atribua um cubo ao Body. O Body conserva a colisÃ£o; os painÃ©is e a porta sÃ£o apresentaÃ§Ã£o sem colisÃ£o prÃ³pria. Desative essa opÃ§Ã£o ao usar arte definitiva.
6. Para persistÃªncia, mantenha SaveComponent com identidade estÃ¡vel no esconderijo e no personagem integrado.

O asset `/Game/_Project/Threat/Data/DA_CabinetTransition` usa entrada de 1,2 s, saÃ­da de 1 s, fases de porta de 20% e abertura de 95Â° em yaw. Foi aplicado aos dois esconderijos das Ã¡reas A/B da demonstraÃ§Ã£o integrada e ao Blueprint BP_ThreatHidingSpot. Os protÃ³tipos sÃ£o funcionais, sem mÃ£os, animaÃ§Ãµes esquelÃ©ticas ou Ã¡udio novo de dobradiÃ§a.

## Saves

Entrada em andamento Ã© normalizada para Hidden no carregamento; saÃ­da em andamento Ã© normalizada para Outside. A restauraÃ§Ã£o nÃ£o repete movimentos nem deixa a porta entreaberta. A orientaÃ§Ã£o anterior da cÃ¢mera tambÃ©m Ã© salva para a saÃ­da posterior. NÃ£o hÃ¡ referÃªncias a atores temporÃ¡rios no arquivo: usa-se o GUID persistente do esconderijo.

Os campos adicionais tÃªm versÃ£o de snapshot. Ao carregar snapshots antigos que nÃ£o contÃªm esses campos, a versÃ£o volta ao comportamento anterior, inclusive em carregamento dentro da mesma sessÃ£o. Isso nÃ£o substitui a compatibilidade de conteÃºdo dos mapas: um esconderijo persistente removido continua sendo um erro de referÃªncia.

## ValidaÃ§Ã£o

Resultados da rodada em [Validation.md](Validation.md). A automaÃ§Ã£o usa pastas de teste separadas dos saves do usuÃ¡rio. Os protocolos Hiding.Runtime e Hiding.ColdRead exigem flags explÃ­citas e UserDir isolada; executar a suÃ­te geral apenas informa sua disponibilidade.

## Referências de arquitetura

- [State — Refactoring.Guru](https://refactoring.guru/design-patterns/state): comportamento condicionado ao estado. Nesta implementação, quatro estados cabem em uma máquina de estados com enum, sem uma classe para cada estado.
- [Observer — Game Programming Patterns](https://gameprogrammingpatterns.com/observer.html): notificações entre participantes. Os delegates notificam os adaptadores e a apresentação sem obrigar o componente de interação a conhecer esses consumidores.

## Roteiro de conferência manual

Abra `/Game/Maps/FrameworkIntegrated_Demo` e inicie uma partida. Aproxime-se de um dos armários sinalizados e use Interagir (E por padrão; o remapeamento existente é respeitado). Durante a entrada, o movimento e o olhar ficam bloqueados. Após o fechamento, o olhar limitado funciona pela abertura do protótipo. Interagir novamente inicia a saída. Abra a pausa no meio de uma transição para conferir sua suspensão e retomada. O inimigo continua perigoso enquanto o jogador entra ou sai, e uma entrada presenciada continua permitindo captura.

Não use os saves temporários da automação como progresso de trabalho. O roteiro automatizado desativa os inimigos somente na instância de teste para isolar a persistência; a configuração autoral mantém o perseguidor ativo.
