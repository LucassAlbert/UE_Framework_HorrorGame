# Áudio e preferências de volume

Escopo acordado: áudio e volumes; pausa por som no Data Asset; prévia imediata; gravação somente em Aplicar; fechar sem aplicar restaura os últimos valores aplicados. Preferências independentes dos cinco slots de progresso. Resultados executados devem ser consultados em [Validation.md](Validation.md).

## Organização

`Plugins/HorrorAudio` é um módulo de runtime, sem dependências de HorrorCore, HorrorUI, inventário ou SaveSystem. `UHorrorAudioSubsystem`, com ciclo de vida da GameInstance, coordena reprodução e volumes. `UHorrorAudioEmitterComponent` permite compor fontes sonoras em atores existentes; `AHorrorAudioEmitter` é uma composição pronta. A interface de volumes e o objeto interativo de demonstração ficam no módulo do projeto.

Componentes mantêm capacidades separadas ([Component](https://gameprogrammingpatterns.com/component.html)); os delegates notificam mudanças e falhas sem inserir referências à UI no plugin ([Observer](https://refactoring.guru/design-patterns/observer)). Não há polling por Tick para atualizar volumes ou reconstruir a tela.

## Definições e reprodução

Crie um Data Asset `UHorrorSoundDefinition` e configure:

- `Sound`: referência sob demanda ao SoundWave ou SoundCue.
- `Channel`: Ambience, Effects, Interface ou Music. Master é um controle geral, não uma categoria de som.
- `PausePolicy`: PauseWithWorld ou ContinueWhilePaused, independentemente da categoria.
- `Volume`, `Pitch`, `FadeInSeconds` e `FadeOutSeconds`.
- `bSpatialized`: som 3D. As distâncias/formato podem ser configurados em `SpatialSettings` ou, selecionando `bUseAttenuationAsset`, em um USoundAttenuation externo.

Loop é uma propriedade autorada no SoundWave/SoundCue. Para loops que precisam manter sua linha do tempo ao silenciar o volume, configure VirtualizationMode como PlayWhenSilent no som. O gerenciador não modifica assets sonoros compartilhados durante o jogo.

`PlaySound(WorldContextObject, Definition, Location, AttachTo)` devolve um GUID que representa o pedido pendente ou a reprodução. O som e a atenuação externa são carregados assincronamente. `OnPlaybackStarted` confirma a criação da reprodução; `OnPlaybackFailed` informa falha posterior. Um GUID inválido indica rejeição imediata. `StopSound` cancela também pedidos ainda carregando; o parâmetro de fade permite término gradual ou imediato. `StopAllSounds` encerra as reproduções desse subsystem.

Use AttachTo para fontes que acompanham um componente. O fim do ator de attachment cancela seus pedidos. O emitter chama Stop em EndPlay. A limpeza do mundo cancela carregamentos e destrói componentes pertencentes àquele mundo; callbacks tardios não recriam sons cancelados. As referências dos assets/componentes ativos são rastreadas pelo coletor de lixo.

Cada reprodução passa por uma SoundClass transitória de seu grupo, independente das classes/mixes antigos do SimpleSettingsMenu. O ganho aplicado ao componente é `Definition.Volume * Master * ChannelVolume`; o fade da reprodução é adicional. Isso controla sons já ativos e novos pedidos, sem modificar volumes nos assets originais. A política ContinueWhilePaused utiliza a flag de áudio de UI da engine, inclusive quando a categoria é música/ambiente/efeito.

Todo áudio do framework deve entrar por PlaySound ou pelos emitters. Chamadas diretas de PlaySound/SpawnSound do Unreal e fontes de terceiros não são interceptadas automaticamente; seus consumidores precisam ser integrados à API. A separação evita um gerenciador global que varre todos os atores ou componentes por frame.

## Preferências e tela

O arquivo versionado é `Saved/Config/HorrorAudioPreferences.json`, relativo à pasta de usuário do jogo. Não é um slot de SaveSystem. A leitura aceita somente a versão e os cinco valores válidos; um arquivo ausente usa os valores iniciais e um arquivo inválido é preservado com diagnóstico no log.

`BeginVolumeEdit(Owner)` concede uma edição exclusiva. `PreviewVolumes` atualiza o áudio e dispara `OnVolumesChanged`, sem escrever arquivo. `ApplyVolumes` grava primeiro em um arquivo temporário exclusivo, substitui o arquivo de preferências e só então confirma os valores aplicados. `CancelVolumeEdit` restaura os valores aplicados mais recentes. Se houve Aplicar e depois novos ajustes, fechar descarta apenas esses ajustes posteriores. A tela chama cancelamento também em sua saída/destruição.

`UAudioSettingsScreen` apresenta volume geral, ambiente, efeitos, interface e música, sliders, botões de ajuste, testes sonoros, Aplicar e Voltar. Cada linha tem seu próprio widget; a tela mantém a transação e os exemplos de áudio. Os testes sonoros são configurados em `PreviewSounds`; use definições com ContinueWhilePaused para ouvi-los nos menus pausados. A reprodução de teste anterior para ao iniciar outra e todas são encerradas ao sair da tela.

HorrorUI ganhou uma entrada `AudioSettings`, acrescentada ao fim do enum existente para preservar os valores anteriores, e `OpenAudioSettingsUI`. `DA_UIConfig.AudioSettings` aponta para `/Game/_Project/Audio/UI/WBP_AudioSettings`, um Blueprint vazio que usa a apresentação nativa. O botão Áudio de `WBP_MainSettings` abre essa tela pela pilha do framework. As outras configurações do SimpleSettingsMenu foram preservadas; sua inicialização legada continua atendendo os consumidores existentes.

## Demonstração e assets

Abra `/Game/Maps/FrameworkAudio_Demo`. É uma cópia da demonstração de diálogos, com dois emitters automáticos e um cubo interativo próximo ao início. `Audio_Music_Demo` continua tocando durante a pausa; `Audio_Ambience_Demo` acompanha a pausa do mundo. Interaja com `Audio_Effect_Demo` para reproduzir um efeito 3D. As políticas e categorias estão nos Data Assets de `/Game/_Project/Audio/Data`; não são regras fixas dessas categorias.

Os cinco botões Ouvir teste usam quatro definições próprias com ContinueWhilePaused. O teste Geral reutiliza o som de Interface, portanto também é afetado pelo volume de Interface. Um novo teste interrompe o anterior; Parar teste e sair da tela também encerram a reprodução.

Os SoundWaves em `/Game/_Project/Audio/Sounds` são cópias de conteúdo já disponível: `S_Ambience_Demo` veio de `/Game/SimpleSettingsMenu/Sounds/Ambient/AMB_Birds`; `S_Music_Demo`, de `/Game/SimpleSettingsMenu/Sounds/Music/MUS_Music_Batlle`; `S_Effect_Demo`, de `/Game/SimpleSettingsMenu/Sounds/SFX/SFX_RevolverShot`; e `S_Interface_Demo`, de `/Engine/VREditor/Sounds/UI/Click_on_Button`. Os originais não foram modificados. São sons provisórios para demonstrar os controles, sujeitos às licenças de seus conteúdos de origem. Música e ambiente são loops; efeito e interface são disparos únicos. As cópias usam PlayWhenSilent. O mapa foi incluído em MapsToCook.

## Reutilização e limites

Para outro projeto C++ compatível, leve `Plugins/HorrorAudio`, habilite o plugin e declare HorrorAudio no Build.cs consumidor. Adicione emitters ou chame o subsystem com definições do seu jogo. A tela deste projeto depende de HorrorUI; o plugin de áudio funciona sem ela e oferece a transação de volumes para outra apresentação.

Preferências não retornam a valores antigos ao carregar um save de progresso. Não há restauração da posição temporal de músicas/sons, mixagem adaptativa de perseguição, dublagem, sistema de passos ou remapeamento/configurações gerais nesta etapa. Cancelamento de carregamentos, volumes e estado dos emitters não substituem testes auditivos e espaciais com um dispositivo real.
