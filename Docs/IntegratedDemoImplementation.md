# Demonstração integrada — etapa 8

Implementação em 31/10/2026, Unreal 5.8.3 / Windows. Demonstração funcional com geometria, personagens e interfaces de protótipo. O alvo de 10–15 minutos é uma referência de ritmo; não foi medido em uma partida manual.

## Como iniciar

No menu principal, Novo jogo abre `/Game/Maps/FrameworkIntegrated_Demo`. Continuar preserva sua regra: carrega o save manual mais recente de qualquer mapa, que pode pertencer a uma demonstração anterior. Novo jogo e Reiniciar não apagam os cinco slots.

Para abrir diretamente no Editor, use esse mapa persistente. Prepare a navegação com Build Paths antes de iniciar PIE, como no roteiro validado. As áreas são carregadas pelo coordenador em runtime; abrir somente uma área isolada não configura a partida completa.

## Percurso e regras

| Momento | Ação e resultado |
| --- | --- |
| Início | Jogador na ala de manutenção; inimigo já ativo, com patrulha, visão, audição e captura. Divisórias oferecem bloqueio visual e caminhos alternativos. |
| Vigia | Conversa sequencial explica ruídos, fusíveis, chave e esconderijos. O mundo continua ativo, com movimento e câmera manuais bloqueados; perigo pode interromper a conversa. |
| Registro | Documento de três páginas abre ao coletar, explica a rota e fica na coleção sem ocupar slots. A primeira abertura registra a leitura. |
| Coleta | Duas partes da chave, dois fusíveis e lanterna: cinco itens cabem nos cinco slots. Eles ficam em posições diferentes da ala. |
| Inspeção | Abrir com sucesso a inspeção de um item registra o exame. Nome, descrição, rotação e zoom usam o sistema existente. Não basta apenas selecionar o item. |
| Combinação | As duas partes são consumidas e produzem a chave da saída. Não há uma chave pronta colocada no cenário. |
| Painel | Cada encaixe aceita seu fusível. Dois encaixes preenchidos destrancam a porta; é preciso interagir para abrir. |
| Travessia | O corredor persistente conecta as alas por streaming. Passagens de preparação bloqueiam uma área até seu estado estar pronto. A IA usa o adaptador de residência existente. |
| Fuga | Esconderijos nas duas alas permitem perder a perseguição quando a entrada não foi vista. O inimigo pode capturar se testemunhar a entrada. |
| Saída | Use a chave combinada na porta, consumindo-a. Interagir com a saída apresenta “Demonstração concluída”, Reiniciar e Voltar ao menu. |

Terminais manuais existem nas duas alas e no corredor. A gravação usa a seleção dos cinco slots e confirmação de sobrescrita existentes. Captura usa o Game Over já integrado ao save global. Inventário, documentos, objetivos e inspeção preservam suas regras de pausa e retorno.

Os seis objetivos são conversar, ler, inspecionar, restaurar o painel, alcançar a ala leste e escapar. O diário aceita fatos produzidos antes de um objetivo ficar visível. A saída exige conversa concluída, leitura, inspeção pronta, puzzle resolvido e visita à ala leste, além de estar destrancada. Isso impede concluir a demo apenas atravessando o cenário ou obtendo a chave fora do percurso. A sequência orienta a exploração sem descartar ações feitas antecipadamente.

## Organização dos mapas e dados

- Persistente: `/Game/Maps/FrameworkIntegrated_Demo` — corredor, porta/puzzle, dois encaixes, terminal, navegação dinâmica, coordenador, gates de preparação, música e ambiente.
- `/Game/Maps/Integrated/IntegratedArea_A` — vigia, registro, cinco pickups, divisórias, esconderijo, terminal e spawner do perseguidor.
- `/Game/Maps/Integrated/IntegratedArea_B` — visita da ala leste, divisória, esconderijo, terminal, rádio interativo e saída.
- `/Game/_Project/Integrated/Data` — itens/receita, diálogo, documento, puzzle, seis objetivos, inimigo e definição do mundo.
- `/Game/_Project/Integrated` — personagem, inimigo, GameMode e menu próprios.

As demonstrações anteriores permanecem disponíveis. Os três mapas novos foram acrescentados a MapsToCook. DA_UIConfig passa a apontar para a cópia do menu cujo Novo jogo referencia a demonstração integrada; o menu original permanece disponível.

Somente os mapas novos receberam ajuste de luz: as fontes herdadas de 4000 candelas deixavam a imagem saturada com a exposição global desativada. Foram configuradas em 350 lúmens e as placas receberam cor para contraste. A configuração global de exposição não foi alterada.

## Código e arquitetura

`AIntegratedDemoCharacter` deriva do personagem que já reúne inventário, inspeção, documentos, diálogo, objetivos e ameaça. Seu único adaptador adicional observa OnStateChanged da inspeção e registra `Demo.Item.Inspected` quando o preview está Ready. Fechar, falhar ou apenas iniciar o carregamento não conclui esse objetivo; restauração de save também não inventa um novo exame.

`AIntegratedDemoExit` deriva da porta de chave existente, reutilizando a transação de uso/consumo. Consulta os fatos exigidos no diário, valida jogador, mundo, distância, estado de jogo e ausência de operação de save. Só conclui uma vez por apresentação e registra `Demo.Exit.Reached` depois de criar a tela com sucesso. Carregamento e EndPlay removem sua apresentação e liberam referências.

`UDemoCompletionScreen` usa a pilha de HorrorUI. A pausa impede que o jogador seja capturado atrás da tela. Esc/B são consumidos, evitando retomar uma partida já concluída. Reiniciar usa o fluxo existente para reabrir o mapa atual; Voltar ao menu abre BootScene. A navegação dos botões utiliza o foco já implementado no framework.

Essas três classes ficam no módulo do projeto, em Demo/IntegratedDemo.h e .cpp. Os plugins reutilizáveis não receberam regras ou dependências da demonstração. Data Assets contêm conteúdo e condições; composição de componentes e eventos conecta as capacidades. Porta e encaixes ficam no mesmo nível persistente, evitando referências diretas entre áreas descarregáveis.

O estado durável continua no SaveSystem: fatos/objetivos, histórico de conversa, coleção/leitura, inventário/equipamento, puzzle/encaixes, portas, esconderijos e memória do inimigo. Atores colocados têm chaves estáveis com prefixo Integrated; o spawner fornece identidade estável ao inimigo móvel no nível persistente. Widgets, sessão de conversa e apresentação de vitória são transitórios. A demo não grava automaticamente ao vencer.

## Validação e reprodução

Resultados e limitações estão em [Validation.md](Validation.md). O novo teste `HorrorFramework.IntegratedDemo.Playthrough` usa os adaptadores reais de conversa, documento, inventário, seleção de encaixes, terminal, carregamento e saída. As reposições controladas do personagem aceleram o roteiro; não medem tempo de jogo ou dificuldade.

O protocolo exige `-IntegratedDemoAutomation`, mapa integrado, UserDir exclusiva contendo HorrorIntegratedTest e cinco slots vazios. Ele recusa sobrescrever saves existentes. Para captura de inspeção/conclusão durante execução com renderização, acrescente `-IntegratedDemoCapture`.

O inimigo é confirmado ativo no início e depois desabilitado somente na cópia runtime desse roteiro determinístico, para isolar transações e restauração. Um roteiro PIE separado mantém a IA ativa e verifica deslocamento real na patrulha. O teste verifica também um caminho completo de navegação pelo corredor após abrir a porta. Isso não substitui uma partida manual de fuga, captura e esconderijo neste layout.

Permanecem para validação manual: ritmo/duração, dificuldade com perseguição ativa durante todo o percurso, controle físico na tela nova, todas as condições de interrupção e reinício de processo usando um save desta demonstração. A infraestrutura de reinício com áreas descarregadas teve validação própria na etapa 7. Shipping, arte final, animações de esconderijo e novos sistemas de sustos não foram acrescentados nesta etapa.
