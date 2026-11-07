# IA e portas — etapa 12

## Comportamento

O inimigo pode usar uma porta destrancada durante perseguição, investigação e busca. Ao chegar ao ponto de travessia, para, aguarda o atraso configurado e abre a porta. A abertura continua instantânea, como nas portas existentes. O inimigo atravessa por navegação e colisão normais e deixa a porta aberta. A patrulha não abre portas fechadas nesta versão.

Portas trancadas por chave ou puzzle permanecem bloqueadas. O navegador procura outra rota válida. Sem uma rota completa, a investigação aceita um caminho parcial até o lado acessível; a busca começa ao redor da posição alcançada, sem atravessar a tranca ou revelar o jogador através dela.

Durante a espera, o jogador pode abrir a porta e antecipar a passagem. Enquanto um agente usa o vínculo de navegação, o jogador não pode fechar a porta aberta sobre ele. Após a travessia, o uso normal é liberado. Captura continua dependendo das verificações existentes de alcance e visibilidade.

## Separação de responsabilidades

- `IHorrorAIDoor`: contrato de consulta de estado/permissão e abertura explícita. Abrir duas vezes mantém a porta aberta; não alterna o estado.
- `UHorrorDoorNavigationComponent`: vínculo de navegação bidirecional, validação da configuração e registro dos agentes em travessia.
- `UHorrorDoorTraversalDefinition`: atraso, alcance de interação e prazo máximo de travessia.
- `AHorrorEnemyController`: estados transitórios None, Waiting e Crossing; usa o timer de raciocínio existente, sem Tick ou timer independente por porta.
- `AInventoryKeyDoor`: adaptador específico do projeto, com a tranca, apresentação e persistência já existentes. Atualiza o vínculo ao mudar o estado.
- `AIntegratedDemoExit`: rejeita abertura por IA, preservando a condição de conclusão da demonstração.

HorrorAI não depende dos plugins de inventário, puzzle ou save. `NavigationSystem` tornou-se dependência pública porque o header público do componente herda uma classe desse módulo. O contrato permite adaptar outra implementação de porta sem ensinar a IA a procurar uma classe concreta.

A separação segue o princípio de adaptação por interface descrito em [Adapter](https://refactoring.guru/design-patterns/adapter). A ação curta usa um enum de estados, conforme a alternativa simples discutida em [State](https://gameprogrammingpatterns.com/state.html); não exige criar uma classe para cada fase.

## Configurar uma porta

1. Implemente `HorrorAIDoor` no ator. As consultas devem refletir a tranca real. A abertura deve validar agente e permissões e ser idempotente.
2. Adicione `HorrorDoorNavigationComponent` e atribua um Data Asset do tipo `HorrorDoorTraversalDefinition`. `Definition = None` desativa explicitamente essa capacidade e preserva os assets antigos.
3. Posicione StartOffset e EndOffset em lados opostos, próximos ao piso navegável. Os valores são centímetros relativos à orientação do ator, independentes de sua escala. Escala zero e pontos coincidentes são inválidos.
4. Garanta NavMesh nos dois lados e passagem física suficiente para a cápsula com a folha aberta. O vínculo não teleporta o inimigo nem desativa colisões.
5. Chame `RefreshDoorNavigation` depois de alterar tranca, transformação ou configuração em runtime. A porta nativa já faz isso nas alterações que controla.

Na demonstração integrada, `CorridorPuzzleDoor` usa `/Game/_Project/Threat/Data/DA_DoorTraversal`: espera de 0,8 s, alcance de 350 cm e prazo de 5 s. Os offsets são (-180, 100, -150) e (180, 100, -150), resultando em pontos próximos ao piso. O deslocamento lateral de 100 cm evita a folha do protótipo, que gira pelo centro. Não copie esses pontos para uma malha com dimensões diferentes sem conferir o espaço navegável.

## Pausa, interrupções e save

A espera usa tempo do mundo e congela durante pausa. `PauseMove` mantém a solicitação de navegação e zera a velocidade; uma parada que aborte a rota não deve ser adicionada nesse ponto.

Troca de estado, desativação, remoção do componente, perda de permissões, fim do prazo e restauração cancelam a interação. Referências não proprietárias são fracas; a limpeza ocorre antes dos callbacks do vínculo para evitar reentrada. O prazo evita prender a IA indefinidamente numa passagem obstruída.

O save mantém os estados persistentes existentes da porta e do inimigo, sem serializar caminhos ou referências transitórias de navegação. Carregar durante a espera restaura a porta fechada e elimina a ação antiga. A IA restaurada pode se aproximar novamente e começar uma nova espera completa. Um callback antigo nunca deve abrir a porta depois do carregamento.

## Validação

`HorrorFramework.Threat.DoorContractAndRestore` cobre contrato, valores inválidos, escala, permissões, espera, abertura repetida, cancelamento e restauração. `HorrorFramework.Doors.Runtime` usa percepção e navegação reais para perseguir, esperar, pausar, abrir, cruzar, restaurar um snapshot e respeitar uma tranca, incluindo uma rota alternativa. `HorrorFramework.Doors.ColdRead` lê o snapshot em outro processo e verifica nova aproximação e travessia.

Execute pela raiz com `Scripts/Validate-Framework.ps1 -Suite Doors -RunId Portas01 -Render`; depois use o mesmo RunId com `-Cold`. As duas execuções usam slots e preferências temporários separados dos dados pessoais. Resultados desta etapa estão em [Validation.md](Validation.md).

Não há arrombamento, animação de mão, fechamento automático nem controle de fila/prioridade para vários inimigos nesta etapa. O tamanho da passagem e o posicionamento dos pontos continuam sendo responsabilidade da autoria do mapa.
