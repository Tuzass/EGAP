# EGAP
O Electrical Grid Alert Protocol (EGAP) é um protocolo para redes de sensores sem fio (WSNs).

## Compilação
Na pasta root (que contém o Makefile), o comando 'make' compila os arquivos na pasta src para os programas client e server. O comando 'make clean' deleta estes programas.

## Execução
Para executar o cliente, basta executar o comando
>./client IP porta_servidor_status porta_servidor_localizacao

Para executar o servidor, basta executar o comando
>./client IP porta_p2p porta_clientes

Para entender melhor o que estas portas significam e como o sistema funciona, leia a [Documentação](https://github.com/Tuzass/EGAP/blob/main/Documentação.pdf).
