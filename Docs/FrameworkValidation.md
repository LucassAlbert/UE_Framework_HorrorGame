# Validação do framework — 05/11/2026

## Escopo

Validar a composição atual dos sistemas, o percurso da demonstração e as interrupções por captura/carregamento. O trabalho começou no commit `7ac8c62`, com o repositório limpo. Os testes usam diretórios próprios; nenhum slot de progresso pessoal foi utilizado. O projeto e a engine associados são HorrorGameFramework / Unreal 5.8.3, Windows.

## Problema encontrado e correção

A perseguição real até um esconderijo presenciado revelou um caso em que o inimigo parava a aproximadamente 120,26 cm do ponto de entrada, enquanto o alcance de captura era 120 cm. A memória da entrada permanecia correta, mas a aproximação encerrava cedo; o inimigo esperava até o tempo de busca expirar, permitindo escapar de uma entrada presenciada.

`AHorrorEnemyController::RequestDestination` agora limita a tolerância de chegada durante Chase ao menor valor entre ArrivalRadius e metade de CaptureDistance. Isso deixa margem para o seguidor de caminho e permite entrar no alcance real de captura. A captura continua exigindo proximidade, visão válida e permissões do alvo; nenhum Game Over é disparado pelo simples término da rota.

A correção ocupa o módulo HorrorAI. Não exige dependências novas, mudanças nos mapas, novas opções de dificuldade ou aumento artificial do alcance de captura.

## Cobertura nova

`HorrorFramework.Validation.LivePursuit` percorre, com percepção, navegação e captura reais:

1. Confirma o inimigo autoral habilitado e grava um estado anterior à perseguição.
2. Posiciona o perseguidor diante do jogador e espera a visão iniciar Chase.
3. Valida uma rota completa até a entrada do armário e entra sob observação.
4. Grava o jogador já escondido junto à memória da entrada presenciada.
5. Espera a IA chegar e abrir o Game Over, verificando limpeza da ocupação e da transição.
6. Aciona Carregar último save na tela real e espera uma segunda captura, após restaurar a memória do inimigo.
7. Recarrega o estado anterior e entra atrás da divisória, sem ser visto.
8. Mantém um inimigo próximo por dois segundos, confirmando a proteção do jogador já escondido.
9. Inicia a saída e espera uma nova captura pelo inimigo ativo.

O fixture usa posições controladas e reduz a velocidade de uma cópia runtime da definição para a primeira entrada terminar antes da aproximação. Não modifica o Data Asset autoral. Após carregar, o inimigo pode ser recriado com os valores autorais; a rota de patrulha é esvaziada na instância de teste junto ao armário para isolar o último caso. Não se trata de uma medição de dificuldade ou de uma partida humana de 10–15 minutos.

## Repetir a validação

O roteiro [Validate-Framework.ps1](../Scripts/Validate-Framework.ps1) aceita as suítes Regression, Integrated, Streaming, Controls, Events, Hiding e Live. Cada suíte recebe uma UserDir isolada e um relatório próprio, e o roteiro recusa reutilizar diretórios de uma execução inicial. Um processo sem relatório, sem testes concluídos ou com falhas retorna erro. Avisos são exibidos no resultado.

Exemplos, a partir da raiz do repositório:

```powershell
.\Scripts\Validate-Framework.ps1 -Suite Regression
.\Scripts\Validate-Framework.ps1 -Suite Integrated -RunId Percurso01 -Render
.\Scripts\Validate-Framework.ps1 -Suite Live -RunId Perseguicao01 -MaxFPS 30
.\Scripts\Validate-Framework.ps1 -Suite Streaming -RunId Areas01
.\Scripts\Validate-Framework.ps1 -Suite Streaming -RunId Areas01 -Cold
```

O mesmo RunId liga uma execução inicial à leitura em processo novo. Cold está disponível para Streaming, Controls, Events, Hiding e Doors. EngineRoot permite indicar outra instalação compatível de UE 5.8. OutputRoot escolhe o local de relatórios e diretórios temporários. PackageRoot deve apontar à pasta Windows do pacote Development; a regressão geral permanece no Editor porque inclui testes EditorContext.

Render cria capturas quando a suíte já oferece esse recurso. MaxFPS limita a taxa de quadros pelo console. Nas suítes que exercitam taxas de quadros, o roteiro desativa o timestep fixo somente naquele processo com um override de configuração. A suíte Controls usa a configuração real do projeto e nenhum comando de limite externo, pois testa as próprias opções de Vídeo de 30 e 60 FPS. O limite não é um benchmark de desempenho.

## Limites

A automação de percurso usa os adaptadores reais e reposiciona o personagem para acelerar o roteiro. Ela desativa a captura depois de confirmar o inimigo habilitado, enquanto a suíte Live cobre o perigo real separadamente. Não substitui uma partida manual contínua para avaliar ritmo, dificuldade e sensação dos comandos. Controle físico, Shipping e migração de arquivos históricos continuam fora desta rodada. Não foi feita uma análise de desempenho com Unreal Insights.

Resultados, avisos e evidências desta rodada em [Validation.md](Validation.md).


## Configuração global de FPS identificada

Na inspeção inicial, DefaultEngine.ini continha bUseFixedFrameRate=True e FixedFrameRate=60. Essa opção força o timestep e prevalece sobre t.MaxFPS no cálculo do limite do tick da engine. A primeira execução solicitada a 15 FPS ainda registrava aproximadamente 60 quadros por segundo; ela não foi considerada prova de baixa taxa de quadros. O roteiro foi corrigido para usar um override apenas no processo de teste. A rodada AuditPkgLive15Actual registrou 46 quadros em aproximadamente 3,07 segundos entre mensagens e passou.

O usuário autorizou desativar a simulação fixa global. DefaultEngine.ini passou a usar bUseFixedFrameRate=False. O valor FixedFrameRate=60 fica inativo, e o limite é aplicado por GameUserSettings a partir da seleção em Vídeo. A suíte Controls verifica pelo ComboBox real que as opções de 30 e 60 FPS chegam ao valor de preferências e à variável t.MaxFPS, além de exigir o timestep global desativado.


## IA e portas

A suíte Doors valida a interação com navegação real, espera pausável, abertura, travessia, rewind de save e respeito à tranca com e sem rota alternativa. Execute `-Suite Doors -RunId Portas01 -Render` e depois `-Suite Doors -RunId Portas01 -Cold`. `PackageRoot` permite executar os mesmos protocolos no pacote Development.
