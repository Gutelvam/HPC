# Problema Job-Shop - Implementação Sequencial e Paralela

Este projeto implementa soluções para o Problema de Escalonamento Job-Shop (JSSP), de acordo com o trabalho prático de Computação de Alto Desempenho. São fornecidas duas implementações: uma sequencial e outra paralela utilizando OpenMP para partilha de memória.

## Relatório Técnico

Este README serve como relatório técnico completo do projeto, abordando os três principais tópicos solicitados no enunciado:
- **A. Implementação Sequencial**: descrição do algoritmo e sua implementação
- **B. Implementação Paralela**: particionamento, comunicação, exclusão mútua e análise
- **C. Análise de Desempenho**: medições, comparações e avaliação de speedup

## Estrutura do Projeto

.
├── data/                       # Arquivos de entrada para os testes
│   ├── jobshop_small.jss      # Conjunto de dados pequeno (3 jobs, 3 máquinas)
│   ├── jobshop_medium.jss     # Conjunto de dados médio (6 jobs, 6 máquinas)
│   └── jobshop_huge.jss       # Conjunto de dados grande (25 jobs, 25 máquinas)
├── logs/                       # Diretório para logs de execução
│   ├── *_execution_times.txt  # Tempos médios de execução
│   ├── sequence.txt       # Sequências de operações executadas
│   └── timing.txt         # Tempos detalhados por thread
├── mappings/                   # Arquivos de mapeamento para visualização
│   ├── mapping_small.txt      # Mapeamento para o conjunto pequeno
│   ├── mapping_medium.txt     # Mapeamento para o conjunto médio
│   └── mapping_huge.txt       # Mapeamento para o conjunto grande
├── viz/                        # Diretório para visualizações geradas
├── job_shop_sequential.c       # Implementação sequencial
├── job_shop_parallel.c         # Implementação paralela com OpenMP
├── job_shop_visualizer.py      # Script para visualização dos resultados
├── LICENSE                     # Licença do projeto
├── Makefile                    # Arquivo para compilação
├── README.md                   # Este arquivo
└── performance_analysis.sh     # Script para análise de desempenho

## Algoritmos Implementados

### Algoritmo Sequencial

A implementação sequencial utiliza um algoritmo guloso que escalona as operações uma a uma. O algoritmo seleciona sempre o job que pode começar mais cedo e escalona sua próxima operação não processada. Para determinar quando uma operação pode ser executada, o algoritmo verifica:

1. As restrições de precedência dentro do mesmo trabalho (sequência das operações)
2. A disponibilidade da máquina necessária

O algoritmo mantém:
- Um array de contadores para o número de operações já escalonadas por trabalho
- Um array com o tempo mais cedo possível para iniciar a próxima operação de cada trabalho
- Um mapeamento das ocupações de cada máquina ao longo do tempo

O pseudocódigo do algoritmo sequencial é:

função schedule_jobs_sequential():
  scheduled_ops[MAX_JOBS] = {0}    // Contador de operações escalonadas por job
  earliest_starts[MAX_JOBS] = {0}   // Tempo mais cedo para cada job
  total_ops = num_jobs * num_operations
  scheduled_count = 0

enquanto scheduled_count < total_ops:
    earliest_job = -1
    earliest_time = INFINITO
    
    // Encontra o job que pode iniciar mais cedo
    para cada job j de 0 até num_jobs-1:
        se scheduled_ops[j] < num_operations e earliest_starts[j] < earliest_time:
            earliest_time = earliest_starts[j]
            earliest_job = j
    
    // Obtém a próxima operação deste job
    op = scheduled_ops[earliest_job]
    machine = jobs[earliest_job][op].machine
    duration = jobs[earliest_job][op].duration
    
    // Encontra slot disponível na máquina
    start_time = find_available_time(machine, duration, earliest_starts[earliest_job])
    
    // Escalona a operação
    jobs[earliest_job][op].start_time = start_time
    scheduled_ops[earliest_job]++
    scheduled_count++
    
    // Atualiza o tempo mais cedo para a próxima operação deste job
    earliest_starts[earliest_job] = start_time + duration


### Algoritmo Paralelo

A implementação paralela segue a metodologia de Foster para paralelização:

1. **Particionamento**: O problema é decomposto por trabalhos, distribuindo os jobs entre as threads
2. **Comunicação**: As threads compartilham o estado das máquinas
3. **Aglomeração**: Os trabalhos são agrupados para serem atribuídos às threads
4. **Mapeamento**: As threads são mapeadas em cores do processador usando OpenMP

A implementação paralela utiliza um mecanismo de distribuição round-robin dos jobs entre as threads disponíveis. Para evitar condições de corrida no acesso às máquinas, a implementação utiliza uma região crítica para a verificação e atualização do estado das máquinas.

O pseudocódigo do algoritmo paralelo é:

função schedule_with_strict_division(num_threads):
  scheduled_ops[MAX_JOBS] = {0}    // Contador de operações escalonadas por job
  earliest_starts[MAX_JOBS] = {0}   // Tempo mais cedo para cada job
  op_assigned[MAX_JOBS][MAX_OPERATIONS]  // Mapeamento de operações para threads
  assigned_count = 0

// Atribuir jobs às threads (round-robin)
para cada job j de 0 até num_jobs-1:
    thread = j % num_threads  // Distribuição round-robin
    para cada operação op de 0 até num_operations-1:
        op_assigned[j][op] = thread

enquanto assigned_count < total_ops:
    // Execução paralela com OpenMP
    #pragma omp parallel num_threads(num_threads)
    {
        thread_id = omp_get_thread_num()
        
        // Região crítica para escalonar operações
        #pragma omp critical (scheduler)
        {
            para cada job j de 0 até num_jobs-1:
                // Verificar se este job está atribuído a esta thread
                se op_assigned[j][0] == thread_id:
                    op = scheduled_ops[j]
                    se op < num_operations:
                        // Escalonar a próxima operação deste job
                        machine = jobs[j][op].machine
                        duration = jobs[j][op].duration
                        start_time = find_available_time(machine, duration, earliest_starts[j])
                        
                        jobs[j][op].start_time = start_time
                        scheduled_ops[j]++
                        assigned_count++
                        earliest_starts[j] = start_time + duration
                        
                        // Log da operação
                        log_operation(thread_id, j, op)
                }
        }
    }
}

Esta abordagem paralela garante que cada thread trabalhe com um conjunto específico de jobs, minimizando a contenção, embora ainda exija sincronização ao acessar o estado das máquinas.

## Relatório Detalhado

### A. Implementação Sequencial

#### 1. Descrição do Algoritmo de Atribuição dos Tempos de Início

O algoritmo sequencial implementado utiliza uma abordagem gulosa (greedy) para escalonar as operações. O princípio básico é:

1. Para cada iteração, encontrar o job que pode iniciar sua próxima operação não escalonada mais cedo
2. Para este job, encontrar o primeiro slot de tempo disponível na máquina necessária para a operação
3. Escalonar a operação no slot encontrado
4. Atualizar o tempo mais cedo possível para a próxima operação deste job

Este algoritmo pode ser descrito formalmente como:

Inicializar earliest_starts[j] = 0 para cada job j
Inicializar scheduled_ops[j] = 0 para cada job j
Enquanto houver operações não escalonadas:
Selecionar job j com menor earliest_starts[j] e scheduled_ops[j] < num_operations
Obter próxima operação op = scheduled_ops[j] do job j
Obter máquina m requerida pela operação
Obter duração d da operação
Calcular start_time = encontrar_slot_disponível(m, d, earliest_starts[j])
Escalonar a operação: jobs[j][op].start_time = start_time
Atualizar earliest_starts[j] = start_time + d
Incrementar scheduled_ops[j]


A função `encontrar_slot_disponível(máquina, duração, earliest_start)` verifica todos os slots de tempo já ocupados na máquina e retorna o primeiro slot disponível que pode acomodar a operação sem conflitos com operações já escalonadas.

#### 2. Implementação Sequencial

A implementação sequencial está organizada da seguinte forma:

- **Estruturas de dados**: Utiliza `Operation` para representar cada operação e `JobShopProblem` para encapsular todo o problema
- **Entrada/Saída**: Funções para ler o problema de um arquivo e escrever a solução em outro
- **Escalonamento**: Implementação do algoritmo guloso descrito acima
- **Medição de desempenho**: Sistema de logging que registra tempos de execução

O código é estruturado para garantir que todas as restrições do problema sejam respeitadas:
- Operações de um mesmo job são executadas em sequência
- Cada máquina processa apenas uma operação por vez
- Uma vez iniciada, uma operação não pode ser interrompida

### B. Implementação Paralela com Partilha de Memória

#### 1. Proposta de Particionamento (Metodologia de Foster)

Seguindo a metodologia de Foster, a paralelização foi implementada da seguinte forma:

**a) Decomposição:**
- **Decomposição por dados**: Os jobs são distribuídos entre as threads
- **Granularidade**: Cada thread é responsável por processar um conjunto de jobs completos
- **Objetos de computação**: Jobs (conjunto de operações sequenciais)
- **Objetos de dados**: Estado das máquinas (disponibilidade temporal)

**b) Comunicação:**
- **Padrão de comunicação**: Produtor-consumidor (threads produzem/consomem slots de tempo nas máquinas)
- **Canais de comunicação**: Através de estruturas de dados compartilhadas em memória
- **Tipo de comunicação**: Síncrona em regiões críticas

**c) Aglomeração:**
- **Estratégia de agrupamento**: Round-robin (job_id % num_threads)
- **Balanceamento de carga**: Estático na atribuição inicial
- **Minimização de comunicação**: Cada thread processa todas as operações dos jobs atribuídos a ela
- **Escalabilidade**: Limitada pela contenção nas estruturas compartilhadas

**d) Mapeamento:**
- **Estratégia de mapeamento**: Gerenciada pelo runtime do OpenMP
- **Implementação**: Diretiva `#pragma omp parallel num_threads(num_threads)`
- **Adaptação ao hardware**: Limitação do número de threads para problemas pequenos
- **Balanceamento dinâmico**: Mecanismo de fallback para casos de deadlock

#### 2. Implementação Paralela

A implementação paralela (`job_shop_parallel.c`) utiliza OpenMP para criar múltiplas threads que escalonam operações simultaneamente. Os principais elementos são:

- **Distribuição de trabalho**: Jobs são distribuídos entre threads usando um esquema round-robin
- **Exclusão mútua**: Regiões críticas protegem o acesso às estruturas compartilhadas
- **Logging**: Cada thread registra suas atividades para análise posterior

A função `schedule_with_strict_division()` implementa a estratégia de distribuição de trabalho, garantindo que cada thread processe um conjunto específico de jobs.

#### 3. Características do Programa Paralelo

**a) Variáveis Globais Compartilhadas:**
- **Somente Leitura:**
  - `problem.num_jobs`, `problem.num_machines`, `problem.num_operations`: Dimensões do problema
  - Identificadores de threads e contadores de iteração

- **Leitura e Escrita:**
  - `problem.jobs[][].start_time`: Tempos de início das operações
  - `scheduled_ops[]`: Contadores de operações escalonadas
  - `earliest_starts[]`: Tempos mais cedo para a próxima operação
  - `problem.thread_logs[][]`: Logs de execução

**b) Seções Críticas e Condições de Corrida:**
- **Principal seção crítica**: `#pragma omp critical (scheduler)` protege:
  - Verificação de disponibilidade de máquinas
  - Atualização dos tempos de início das operações
  - Incremento de contadores de operações escalonadas

- **Potenciais condições de corrida sem proteção:**
  - Acesso concorrente à mesma máquina por diferentes threads
  - Múltiplas threads tentando registrar logs simultaneamente

**c) Técnicas de Exclusão Mútua:**
- **Regiões críticas OpenMP**: Protegem seções que manipulam o estado das máquinas
- **Distribuição por job**: Minimiza a contenção ao atribuir jobs completos a threads específicas
- **Fallback sequencial**: Mecanismo para resolver deadlocks em caso de contenção excessiva

### C. Análise do Desempenho

#### 1. Mecanismos de Medição do Tempo de Execução

O sistema de medição de tempo implementado utiliza:

- **Versão sequencial**: Usa `clock()` para medir o tempo de CPU
- **Versão paralela**: Usa `omp_get_wtime()` para medição precisa de tempo de parede
- **Repetições**: Cada execução é repetida 10 vezes para obter médias confiáveis
- **Logging**: Os tempos são registrados em arquivos de log detalhados

Ambas as versões evitam I/O durante a medição de desempenho para não influenciar os resultados.

#### 2. Resultados de Desempenho

**Tamanho dos Problemas:**
- **Small**: 3 jobs × 3 máquinas = 9 operações
- **Medium**: 6 jobs × 6 máquinas = 36 operações
- **Huge**: 25 jobs × 25 máquinas = 625 operações

**Tempos de Execução para o Conjunto Huge:**

| Implementação         | Threads | Tempo Médio (s) | Operações | Tempo por Op (s) |
|-----------------------|---------|-----------------|-----------|------------------|
| Sequencial            | 1       | 0.000500        | 1250      | 4.00×10⁻⁷        |
| Paralela              | 4       | 0.001200        | 1250      | 9.60×10⁻⁷        |

**Análise por Dataset:**

| Dataset | Tamanho     | Sequencial (s) | Paralelo (s) | Speedup | Eficiência |
|---------|-------------|----------------|--------------|---------|------------|
| Small   | 3×3 = 9 ops | ~0.000050*     | ~0.000200*   | ~0.25   | ~6.25%     |
| Medium  | 6×6 = 36 ops| ~0.000100*     | ~0.000500*   | ~0.20   | ~5.00%     |
| Huge    | 25×25 = 625 | 0.000500       | 0.001200     | 0.42    | 10.42%     |

\* Valores estimados com base na proporção do tamanho dos problemas, já que os logs detalhados foram fornecidos apenas para o conjunto "Huge".

**Distribuição de Trabalho entre Threads (Conjunto Huge, 4 threads):**

| Thread ID | Operações Processadas | % do Total | Desvio da Média |
|-----------|----------------------|------------|-----------------|
| 0         | 350                  | 28.0%      | +12.0%          |
| 1         | 300                  | 24.0%      | -4.0%           |
| 2         | 300                  | 24.0%      | -4.0%           |
| 3         | 300                  | 24.0%      | -4.0%           |
| **Total** | **1250**             | **100%**   | **16.0%***      |

\* Calculado como a diferença percentual entre a thread com mais operações e a thread com menos.

**Gráfico de Speedup (estimado):**

Speedup
^
1.0 |
|                    Ideal
0.8 |                   /
|                  /
0.6 |                 /
|                /
0.4 |               /  Real
|              *
0.2 |         *   /
|    *      /
0.0 +----*----+-----+-----+----> Threads
1     4     8    16

#### 3. Análise do Speedup e Eficiência

Para o conjunto "Huge" com 625 operações:

- **Speedup**: 0.42x (a versão paralela é mais lenta que a sequencial)
- **Eficiência**: 10.42% (eficiência = speedup / número de threads × 100%)

**Análise dos Resultados:**

1. **Speedup Negativo**: A implementação paralela apresenta um speedup menor que 1, indicando que a versão sequencial é mais eficiente. Isso é típico de problemas com:
   - Custo de comunicação/sincronização maior que o benefício da paralelização
   - Problema de tamanho insuficiente para compensar o overhead de criação e gerenciamento de threads
   - Possíveis contenções nos acessos às estruturas compartilhadas

2. **Distribuição de Carga**: Existe um desequilíbrio de 16% entre as threads, com a thread 0 processando mais operações (28%) que as demais (24% cada).

3. **Granularidade Fina**: O problema apresenta operações muito rápidas (ordem de microssegundos), o que faz com que o overhead de sincronização supere o ganho de processamento paralelo. A média de tempo por operação no sequencial é de apenas 8.00×10^-7 segundos.

4. **Overhead de Sincronização**: A região crítica que protege a verificação e atualização do estado das máquinas causa contenção significativa.

#### 4. Possíveis Melhorias

Com base nos resultados obtidos, as seguintes melhorias poderiam ser implementadas:

1. **Particionamento por máquinas**: Uma estratégia alternativa seria particionar por máquinas em vez de por jobs, reduzindo a contenção.

2. **Granularidade adaptativa**: Implementar um mecanismo que decide dinamicamente entre execução sequencial ou paralela com base no tamanho do problema.

3. **Redução das regiões críticas**: Minimizar o escopo das regiões críticas ou usar estruturas de dados que permitam maior concorrência.

4. **Lock por máquina**: Implementar um lock individual por máquina em vez de uma única região crítica, permitindo operações paralelas em máquinas diferentes.

5. **Escalonamento dinâmico**: Utilizar a cláusula `schedule(dynamic)` do OpenMP para balancear melhor a carga entre as threads.

## Conclusões

O desenvolvimento e análise das implementações sequencial e paralela do Problema Job-Shop nos permitiu obter várias conclusões importantes:

1. **Eficácia do algoritmo sequencial**: A implementação sequencial apresenta um desempenho satisfatório para os tamanhos de problema testados, com tempos de execução muito baixos mesmo para o conjunto "huge" (25×25).

2. **Desafios da paralelização**: A paralelização deste problema específico apresenta desafios significativos devido à natureza interdependente das restrições de escalonamento.

3. **Overhead de sincronização**: Para problemas pequenos ou com operações muito rápidas, o custo de sincronização pode superar os ganhos de processamento paralelo.

4. **Balanceamento de carga**: Mesmo com a distribuição round-robin, observamos um desequilíbrio na carga de trabalho entre as threads.

5. **Escalabilidade**: Os resultados sugerem que o algoritmo paralelo atual não escalaria bem com o aumento do número de threads, devido à contenção nas regiões críticas.

A análise realizada neste trabalho demonstra a importância de avaliar cuidadosamente a adequação da paralelização para cada tipo de problema. Nem todos os problemas se beneficiam igualmente da computação paralela, especialmente quando as dependências de dados e a granularidade das operações não favorecem a execução concorrente.

Para trabalhos futuros, seria interessante explorar algoritmos alternativos para o JSSP que possam se beneficiar mais da paralelização, como abordagens baseadas em metaheurísticas com populações paralelas (algoritmos genéticos ou colônia de formigas), que normalmente apresentam melhor escalabilidade em ambientes paralelos.

## Descrição do Problema

O problema Job-Shop consiste em escalonar um conjunto de operações (jobs) em um conjunto de máquinas. Cada trabalho é composto por várias operações que devem ser processadas em uma ordem específica. Cada operação requer uma máquina específica por um período determinado. O objetivo é minimizar o tempo total de processamento (makespan) respeitando as seguintes restrições:

- Uma máquina só pode processar uma operação por vez
- As operações de um mesmo trabalho devem ser executadas sequencialmente
- Uma vez iniciada, a operação não pode ser interrompida

## Requisitos

- GCC com suporte para OpenMP
- Python 3.x com matplotlib para visualização
- Sistema operacional Linux ou Windows (com MinGW para Windows)

## Compilação

O projeto inclui um Makefile para facilitar a compilação. Para compilar todos os programas, execute:

```bash
# Compilar versão sequencial
gcc -Wall -O2 -o jobshop_sequential.exe job_shop_sequential.c

# Compilar versão paralela (com OpenMP)
gcc -Wall -O2 -fopenmp -o jobshop_parallel.exe job_shop_parallel.c

# Gerar otimizacao jobshop sequencial
./jobshop_sequential.exe data/jobshop_small.jss jobshopsmall_seq.jss

# Gerar otimizacao jobshop paralela
./jobshop_parallel.exe data/jobshop_medium.jss jobshopmedium_parallel.jss 4
```

## Visualização dos Resultados
O projeto inclui um script Python para visualizar os resultados em formato de gráfico de Gantt:

```bash
python job_shop_visualizer.py --type <tipo> --input_file <arquivo_entrada> --output_file <nome_saida>

```

Onde:

<tipo> é 0 para conjunto pequeno, 1 para médio e 2 para grande
<arquivo_entrada> é o arquivo de saída gerado pela execução do programa
<nome_saida> é o nome do arquivo de imagem a ser gerado

Exemplo:

```bash
python job_shop_visualizer.py --type 0 --input_file jobshopsmall_seq.jss --output_file job_shop_small.png

```