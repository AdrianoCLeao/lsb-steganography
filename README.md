# Least Significant Bit Steganography

## Pré-requisitos

- **GCC** (ou outro compilador compatível com `gcc`)
- **make**
- Em Windows: suporte a linha de comando que reconheça `make` (MinGW, Git Bash, WSL, etc.)


## Principais comandos

| Comando           | Descrição                                                                                   |
|-------------------|---------------------------------------------------------------------------------------------|
| `make` ou `make all` | Compila todos os objetos, gera o executável e as bibliotecas estáticas                      |
| `make run`        | Executa o binário gerado (`bin/main.exe` no Windows ou `./bin/main.exe` no Unix)           |
| `make lib`        | Apenas compila e gera as bibliotecas em `lib/`                                              |
| `make clean`      | Remove completamente os diretórios `bin/` e `lib/`, limpando todos os artefatos de build    |

---

### Exemplos de uso

1. **Build completo**  
   ```bash
   make
   ```

2. **Executar o programa**  
   ```bash
   make run
   ```
   
3. **Limpar artefatos de compilação**  
   ```bash
   make clean
   ```
