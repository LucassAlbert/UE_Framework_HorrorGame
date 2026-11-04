# Controles e câmera — etapa 9

Implementado em 01/11/2026 para Unreal 5.8.3 / Windows. Acesso: Configurações > Controles e câmera, pelo menu principal ou pela pausa. Os resultados executados estão em [Validation.md](Validation.md).

## Comportamento aprovado

- Remapeamento das teclas de movimento e dos comandos Interagir, Agachar, Pausa e Inventário; mouse e botões de controle são aceitos no dispositivo correspondente. A lista contém os 12 comandos existentes nos contextos do projeto.
- Ao atribuir um comando ocupado, as duas ações trocam de tecla/botão, mantendo ambas disponíveis.
- A navegação dos menus permanece fixa: A confirma; B/Esc volta. Os analógicos continuam com movimento e câmera nos papéis originais.
- Sensibilidade X/Y e inversão X/Y independentes para mouse e controle. Sensibilidade é um multiplicador da configuração existente, inicialmente 1, com faixa 0,1–5.
- FOV inicialmente 90°, ajustável de 70° a 110°. Padrões e limites ficam no Data Asset e podem ser definidos por jogo.
- Alterações têm prévia imediata. Aplicar grava; sair sem aplicar restaura os últimos valores aplicados. Restaurar padrões também é uma prévia, sujeita a Aplicar/cancelar.
- Durante a captura, B pode virar um comando de gameplay; não fecha a tela nessa mesma entrada. Esc cancela a captura. Sem entrada, ela termina após aproximadamente dez segundos reais, inclusive com o jogo pausado. O foco retorna ao comando editado. Após a captura, B volta a navegar normalmente.

As teclas/botões são atribuições simples, sem combinações do tipo Ctrl+E ou troca dos analógicos. A inspeção mantém seus controles próprios de rotação/zoom e não recebe o FOV da câmera de gameplay. Interagir e Pausa continuam sendo as ações usadas pelo diálogo.

## Organização

| Elemento | Responsabilidade |
| --- | --- |
| Plugin HorrorControls | Definições, validação, preferências por jogador local, edição, arquivo próprio e cópias dos mapas de input. Depende somente de módulos da engine e Enhanced Input, sem HorrorCore, HorrorUI ou SaveSystem. |
| UControlPreferencesDefinition | ID do perfil, padrões/limites, referência da ação de olhar e comandos com IDs estáveis, dispositivo, ação e tecla original. ImportBindings auxilia a autoria pelos contextos existentes. |
| UHorrorControlSettingsSubsystem | Mantém valores atuais/aplicados, concede a edição a um proprietário por vez, restaura ao cancelar e notifica mudanças. Vive no jogador local, atravessando trocas de mapa. |
| UControlLookModifier | Aplica sensibilidade/inversão no mapeamento do dispositivo de origem, depois dos modificadores existentes. Não usa o último dispositivo detectado como atalho. |
| UControlPreferencesSave | Arquivo de preferências, versão 1, com identidade do perfil e todos os valores. |
| AHorrorCharacter | Registra/remove o contexto de gameplay pelo adaptador, na posse, troca de pawn e encerramento. |
| AHorrorPlayerController | Observa preferências e aplica o FOV na câmera do pawn local, inclusive na prévia pausada e ao restaurar o estado de controle. |
| UControlSettingsScreen | Apresentação, sliders/passos, inversões, captura e Aplicar/padrões/Voltar. Tempo de captura usa relógio real e ticker temporário, removido ao encerrar. |
| UMainSettingsScreen | Mantém a navegação lateral de WBP_MainSettings, hospeda Áudio/Controles no painel direito e gerencia abertura, encerramento e foco de cada página. Copia os estilos do próprio template. |

Composição, eventos e Data Assets conectam as capacidades. O plugin reutilizável não sabe qual é o personagem, a tela, o mapa ou o sistema de save de progresso. A apresentação específica permanece no módulo do projeto.

## Input e câmera

Cada jogador recebe uma cópia privada de seu Input Mapping Context. O remapeamento altera somente as teclas dessa cópia; não modifica os assets compartilhados. Ações, triggers, swizzles de movimento, dead zone, escala e delta de tempo existentes são preservados.

O modificador identifica mouse/controle no próprio mapeamento, antes de seus valores serem combinados pela ação. Isso permite preferências distintas sem inferir o dispositivo a partir da última tecla pressionada. A consulta durante o input usa referência aos valores, evitando copiar o mapa de comandos por frame. Mudanças só de câmera não precisam reconstruir os mapeamentos. Ao mudar teclas, o Enhanced Input é reconstruído com limpeza e proteção contra comandos ainda pressionados.

O FOV é aplicado por evento e na preparação do pawn, sem acrescentar atualização por frame ao controlador. As preferências não são propriedades SaveGame do personagem: carregar um progresso não deve sobrescrevê-las. A prévia de FOV exige uma câmera de jogador existente; ajustes feitos no menu principal serão usados pelo pawn ao iniciar a partida.

## Persistência e falhas

O arquivo é `HorrorControlPreferences_0.sav` para o jogador local principal, separado dos cinco SaveSlot de progresso e do arquivo de áudio. A versão atual foi validada para o uso single-player deste projeto.

Aplicar valida todos os valores e grava o arquivo antes de atualizar os valores aplicados. Falha de gravação informa o erro e não confirma uma aplicação inexistente. A prévia não escreve no arquivo. Fechar, destruir a tela ou trocar de mundo cancela a edição não aplicada.

Valores não finitos, FOV/sensibilidade fora dos limites, comandos inválidos, dispositivo incompatível e conflitos são recusados. Arquivo incompatível/corrompido, versão ou perfil desconhecido usam os padrões e apresentam diagnóstico; o arquivo só é substituído por um Aplicar explícito. Alterações futuras nos IDs/lista de comandos exigem migração ou retorno aos padrões; esta etapa não implementa um migrador de versões.

## Assets e configuração

- `/Game/_Project/Controls/DA_ControlDefaults`.
- `/Game/_Project/Controls/WBP_ControlSettings`, baseado na tela nativa.
- `/Game/SimpleSettingsMenu/Widgets/WBP_MainSettings` é a tela inicial, com Vídeo no template e acesso às duas telas do framework pelos botões Áudio e Controles. Áudio e Controles são incorporados ao painel direito do template, mantendo a coluna de abas visível. A aba ativa fica indicada pelo botão desabilitado. Voltar ou Esc/B fecha Configurações e retorna à tela anterior.
- DA_UIConfig aponta Settings para WBP_MainSettings e referencia separadamente as telas de Áudio e Controles. As rotinas legadas de volume e sensibilidade foram desconectadas do fluxo do template.
- `DefaultGame.ini`, seção `/Script/HorrorControls.HorrorControlsSettings`, aponta Definition para DA_ControlDefaults.
- A pasta de conteúdo Controls foi incluída em DirectoriesToAlwaysCook.

Para outro projeto C++ compatível: copie/ative HorrorControls, declare seu módulo consumidor, crie uma definição com suas próprias ações/IDs/padrões e configure o asset. Registre/remova os contextos de gameplay pelo subsistema. Conecte OnPreferencesChanged à câmera e à apresentação desejadas, ou reutilize os adaptadores de HorrorCore. As telas deste projeto não fazem parte do plugin portátil. A transferência para outro projeto ainda não foi certificada.

## Roteiro manual

1. Em uma partida, abra Pausa > Configurações > Controles e câmera.
2. Mude FOV e sensibilidade; volte sem aplicar. Reabra e confira o retorno aos valores anteriores.
3. Atribua C a Interagir: Agachar deve passar para E. Durante outra captura, Esc deve cancelar sem sair da tela.
4. Aplique e retorne ao jogo. Confira C para interagir, E para agachar, sensibilidade e FOV.
5. Reabra, escolha Restaurar padrões e volte sem aplicar: os valores aplicados devem permanecer. Aplicar os padrões deve gravá-los.
6. Feche/reabra usando a mesma pasta de usuário: os últimos valores aplicados devem ser recuperados.

O usuário informou disponibilidade somente de teclado/mouse nesta rodada. A simulação de gamepad dos testes não equivale a confirmação física de controle. Aspecto visual e fluxo devem ser distinguidos de desempenho, outros dispositivos/plataformas e certificação Shipping.

As notificações de mudança seguem [Observer, de Refactoring Guru](https://refactoring.guru/design-patterns/observer): consumidores se inscrevem sem o plugin conhecer suas classes concretas. A separação de responsabilidades por domínio acompanha a orientação de composição de [Game Programming Patterns](https://gameprogrammingpatterns.com/), sem introduzir hierarquias extras apenas para nomear padrões.


## Abas persistentes (04/11/2026)

O escopo visual aprovado abrange Vídeo, Áudio e Controles. O WBP_MainSettings continua como único modal no gerenciador durante a troca de abas. As páginas recebem seu jogador, gerenciador e host antes de construir a apresentação, por isso podem adaptar a largura das linhas à coluna direita. Uma aba do mesmo tipo já aberta conserva a instância atual.

O host copia as âncoras do painel de Vídeo. Botões, fonte, fundo e estilos de slider e rolagem vêm dos widgets do template. A navegação lateral permanece disponível durante edição e captura. Os botões Aplicar e Restaurar padrões pertencem às respectivas páginas, e o botão Voltar fica na coluna lateral.

Trocar de aba chama OnScreenExit e ReceiveScreenExit antes de remover a página e chama os eventos de entrada da nova. A troca cancela alterações ainda não aplicadas, libera a edição exclusiva, encerra amostras sonoras e cancela captura/ticker de tecla. Voltar ou Esc/B fecha Configurações. Durante a captura de um comando, Esc primeiro cancela a captura e B pode ser atribuído, conforme as regras existentes. Valores já aplicados permanecem vigentes.

UUIScreenBase permite indicar um PresentationHost fraco para encaminhar o fechamento de páginas incorporadas. O ciclo de vida do modal permanece com UIManagerSubsystem, enquanto o ciclo das páginas fica com UMainSettingsScreen. O encerramento do host também libera a página quando a tela é destruída ou o mundo muda.
