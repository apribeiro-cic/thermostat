# Termostato Digital Interativo na BitDogLab

## Descrição
Este projeto implementa um termostato utilizando a placa BitDogLab, integrada a placa Raspberry Pi Pico W de microcontrolador RP2040. O sistema monitora e controla a temperatura, ajustando o aquecimento ou resfriamento com base em um setpoint (temperatura desejada) configurável. O display OLED exibe as informações de temperatura em tempo real e um joystick permite a alteração dos parâmetros de funcionamento.
O projeto utiliza apenas a placa BitDogLab, e, portanto, os componentes são apenas simulados na placa. Por exemplo, o joystick funciona como um sensor de temperatura, ajustada conforme o movimento vertical.

## Funcionalidades
- Alteração de temperatura via ADC
- Ajuste do setpoint e histerese
- Controle de aquecimento e resfriamento
- Interface OLED para exibição dos valores
- Configuração intuitiva via botões e joystick

## Componentes Utilizados
- BitDogLab (RP2040 - Raspberry Pi Pico W)
- Display OLED SSD1306
- Joystick analógico
- LED RGB
- Buzzer
- Matriz de LEDs 5x5
- Botões A, B e do joystick.

## Como Usar
1. **Ligar o sistema** e aguardar a inicialização.
   - A interface deve aparecer no display, mostrando valores como temperatura, setpoint (temperatura desejada) e histerese (margem de erro de temperatura).
2. **Navegação:**
   - **Pressionar o botão A** entra no modo de configuração de temperatura.
     - **Mover o joystick para cima/baixo** altera a temperatura do sistema.
     - **Apertar novamente o botão A** sai do modo de configuração de temperatura.

   - **Pressionar o botão B** entra no modo de configuração de setpoint.
     - **Mover o joystick para cima/baixo** altera a temperatura desejada do sistema.
     - **Apertar novamente o botão B** sai do modo de configuração de setpoint.

   - **Pressionar o botão do joystick** entra no modo de configuração de histerese.
     - **Mover o joystick para cima/baixo** altera a histerese do sistema.
     - **Apertar novamente o botão do joystick** sai do modo de configuração de histerese.

3. O sistema controlará a temperatura de acordo com os valores ajustados.
   - **Ao sair dos modos de configuração**, o sistema voltará ao estado normal e verificará se o valor de temperatura está correto.
   - **Caso não**, entra nos modos de aquecimento ou resfriamento, até que a temperatura esteja dentro do intervalo correto.
     
4. O sistema exibirá feedback sonoro e visual através do Display SSD1306, da Matriz de LEDs, dos LEDs RGB e dos buzzers.
   - **Display**: Mostra todos os valores do sistema, bem como seu estado atual.
   - **Matriz de LEDs**: Mostra um desenho colorido conforme o estado atual do sistema.
   - **LED RGB**: Piscará em vermelho ou azul caso o sistema esteja no estado de aquecimento e resfriamento.
   - **Buzzers**: Emitirão beeps isolados ou periódicos para dar feedback sobre o estado do sistema.
  
   

