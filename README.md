# Implementação do Problema Job-Shop

Este projeto contém implementações sequencial e paralela para o problema Job-Shop conforme especificado no enunciado.

## Arquivos Incluídos

- `job_shop_sequential.c`: Implementação sequencial do algoritmo
- `job_shop_parallel.c`: Implementação paralela usando OpenMP
- `generate_input.c`: Gerador de arquivos de entrada para testes
- `Makefile`: Arquivo para compilação dos programas
- `performance_analysis.sh`: Script para análise de desempenho

## Compilação

Para compilar todos os programas, execute:

```bash
make all
gcc -Wall -O2 -o generate_input generate_input.c
```

## Uso

### Gerador de Entrada

```bash
./generate_input <num_jobs> <num_machines> <output_file>
```

Exemplo:
```bash
./generate_input 100 100 large_input.jss
```

### Versão Sequencial

```bash
./job_shop_sequential <arquivo_entrada> <arquivo_saida>
```

Exemplo:
```bash
./job_shop_sequential large_input.jss sequential_output.jss
```

### Versão Paralela

```bash
./job_shop_parallel <arquivo_entrada> <arquivo_saida> <num_threads>
```

Exemplo:
```bash
./job_shop_parallel large_input.jss parallel_output.jss 8
```

## Análise de Desempenho

Execute o script de análise de desempenho:

```bash
chmod +x performance_analysis.sh
./performance_analysis.sh
```

Este script executa ambas as versões (sequencial e paralela) com diferentes números de threads e mostra os tempos de execução.

## Descrição das Soluções

### Implementação Sequencial

A implementação sequencial utiliza uma abordagem gulosa para escalonar as operações. Para cada trabalho, suas operações são escalonadas em sequência, respeitando as restrições de dependência entre operações do mesmo trabalho e a disponibilidade das máquinas.

### Implementação Paralela

A implementação paralela distribui os trabalhos entre as threads disponíveis usando OpenMP. Cada thread é responsável por escalonar todas as operações de um conjunto de trabalhos. Para evitar condições de corrida no acesso às máquinas, utilizamos locks para garantir exclusão mútua durante a verificação e atualização do estado das máquinas.

#### Particionamento (Metodologia de Foster)

1. **Decomposição**: O problema foi decomposto por trabalhos, onde cada thread processa um conjunto de trabalhos independentes.
2. **Comunicação**: As threads comunicam-se através de estruturas de dados compartilhadas (arrays para controle do estado das máquinas).
3. **Aglomeração**: Os trabalhos são distribuídos dinamicamente entre as threads usando o escalonamento dinâmico do OpenMP.
4. **Mapeamento**: O OpenMP cuida do mapeamento das threads para núcleos de processamento.

#### Variáveis Compartilhadas e Exclusão Mútua

- **Variáveis compartilhadas (leitura/escrita)**:
  - `machine_end_times`: Array que armazena o último tempo ocupado para cada máquina
  - `job_end_times`: Array que armazena o último tempo de operação para cada trabalho
  - `problem.jobs`: Matriz de operações com os tempos de início

- **Seções críticas**:
  - Verificação e atualização do estado das máquinas (proteção com locks)

- **Técnicas de exclusão mútua**:
  - Locks do OpenMP (omp_lock_t) para cada máquina


### Compiling of sequential execution
gcc -Wall -O2 -o jobshop_sequential.exe job_shop_sequential.c
.\jobshop_sequential.exe .\data\jobshop_small.jss jobshopsmall_seq.jss
.\jobshop_sequential.exe .\data\jobshop_medium.jss jobshopmedium_seq.jss
.\jobshop_sequential.exe .\data\jobshop_huge.jss jobshophuge_seq.jss

### Compiling of parallel execution
gcc -Wall -O2 -fopenmp -o jobshop_parallel.exe job_shop_parallel.c
.\jobshop_parallel.exe .\data\jobshop_small.jss jobshopsmall_parallel.jss  4
.\jobshop_parallel.exe .\data\jobshop_medium.jss jobshopmedium_parallel.jss  4
.\jobshop_parallel.exe .\data\jobshop_huge.jss jobshophuge_parallel.jss  4


## Generate vizualizations 

### Parallel
- python job_shop_visualizer.py --type 0 --input_file jobshopsmall_parallel.jss  --output_file job_shop_small_4.png
- python job_shop_visualizer.py --type 1 --input_file jobshopmedium_parallel.jss  --output_file job_shop_medium_4.png
- python job_shop_visualizer.py --type 2 --input_file jobshophuge_parallel.jss  --output_file job_shop_huge_4.png

### Sequential:
- python job_shop_visualizer.py --type 0 --input_file jobshopsmall_seq.jss  --output_file job_shop_small.png
- python job_shop_visualizer.py --type 1 --input_file jobshopmedium_seq.jss  --output_file job_shop_medium.png
- python job_shop_visualizer.py --type 2 --input_file jobshophuge_seq.jss  --output_file job_shop_huge.png