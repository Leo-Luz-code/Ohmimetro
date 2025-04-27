# Interface Homem-Máquina com Display OLED - RP2040 (BitDogLab)

# Descrição

Este projeto foi desenvolvido a placa BitDogLab. Tem como objetivo criar uma interface homem-máquina (IHM) utilizando um display OLED modelo "SSD1306" (resolução 128x64 pixels).

A aplicação exibe informações em tempo real dos potenciômetros do joystick, como a leitura dos eixos X e Y. Também exibe o estado dos botões físicos e do botão do joystick. As informações são mostradas de forma organizada no display OLED via interface I2C, objetivando
o estudo/aprendizado das funções da biblioteca do display.

Observação.
- Implementação do modo BOOTSEL por botão externo (Botão B - GPIO 6). Isto facilita a gravação no desenvolvimento do programa. Quando o projeto for finalizado, deve-se retirar
esta função.
