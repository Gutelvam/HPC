import matplotlib.pyplot as plt
import numpy as np
import matplotlib.patches as patches
import matplotlib.colors as mcolors
import random
import argparse
from matplotlib.patheffects import withStroke

def read_job_shop_file(filename):
    """Lê um arquivo de resultado do Job-Shop."""
    with open(filename, 'r') as file:
        lines = file.readlines()
    
    # A primeira linha contém o makespan (tempo total)
    makespan = int(lines[0].strip())
    
    # As linhas restantes contêm os tempos de início e durações das operações para cada job
    jobs_start_times = []
    job_durations = []
    for i in range(1, len(lines)):
        if lines[i].strip():  # Ignorar linhas vazias
            pairs = lines[i].strip().split()
            start_times = []
            durations = []
            for pair in pairs:
                start, duration = map(int, pair.split(','))
                start_times.append(start)
                durations.append(duration)
            jobs_start_times.append(start_times)
            job_durations.append(durations)
    
    return makespan, jobs_start_times, job_durations

def read_machine_mapping(filename):
    """Lê o arquivo de mapeamento de máquinas gerado pelo programa C."""
    try:
        with open(filename, 'r') as file:
            lines = file.readlines()
        
        # Cada linha contém as máquinas para um job
        job_machine_assignments = []
        for line in lines:
            if line.strip():  # Ignorar linhas vazias
                machines = list(map(int, line.strip().split()))
                job_machine_assignments.append(machines)
        
        return job_machine_assignments
    except FileNotFoundError:
        print(f"Arquivo de mapeamento {filename} não encontrado. Usando mapeamento padrão.")
        return None

def verify_no_overlap(machine_timeframes):
    """
    Verifica se há sobreposição de operações em uma máquina.
    Retorna True se não houver sobreposição, False caso contrário.
    """
    for machine, operations in machine_timeframes.items():
        sorted_ops = sorted(operations, key=lambda x: x['start'])
        for i in range(1, len(sorted_ops)):
            prev_end = sorted_ops[i-1]['start'] + sorted_ops[i-1]['duration']
            curr_start = sorted_ops[i]['start']
            if curr_start < prev_end:
                print(f"Sobreposição detectada na máquina {machine}:")
                print(f"  Operação anterior: job {sorted_ops[i-1]['job']}, op {sorted_ops[i-1]['operation']}, " + 
                      f"início: {sorted_ops[i-1]['start']}, duração: {sorted_ops[i-1]['duration']}, fim: {prev_end}")
                print(f"  Operação atual: job {sorted_ops[i]['job']}, op {sorted_ops[i]['operation']}, " + 
                      f"início: {curr_start}, duração: {sorted_ops[i]['duration']}")
                return False
    return True

def create_default_machine_assignments(jobs_start_times):
    """Cria atribuições de máquinas padrão quando não temos o arquivo de mapeamento."""
    num_jobs = len(jobs_start_times)
    max_operations = max(len(job) for job in jobs_start_times)
    num_machines = max_operations
    assignments = []
    for job_idx, start_times in enumerate(jobs_start_times):
        job_assignments = []
        for op_idx in range(len(start_times)):
            machine = (op_idx + job_idx) % num_machines
            job_assignments.append(machine)
        assignments.append(job_assignments)
    return assignments

def create_job_shop_gantt(jobs_file, mapping_file, output_image):
    """Cria um gráfico de Gantt aprimorado para visualizar os resultados do Job-Shop."""
    makespan, jobs_start_times, job_durations = read_job_shop_file(jobs_file)
    
    # Ler o mapeamento de máquinas
    job_machine_assignments = read_machine_mapping(mapping_file)
    if job_machine_assignments is None:
        job_machine_assignments = create_default_machine_assignments(jobs_start_times)
    
    # Verificar compatibilidade de dimensões
    for job_idx, (start_times, machines) in enumerate(zip(jobs_start_times, job_machine_assignments)):
        if len(start_times) != len(machines):
            print(f"AVISO: Incompatibilidade no job {job_idx}: {len(start_times)} tempos vs {len(machines)} máquinas")
            if len(machines) < len(start_times):
                extra_machines = [m % len(machines) for m in range(len(machines), len(start_times))]
                job_machine_assignments[job_idx].extend(extra_machines)
            elif len(machines) > len(start_times):
                job_machine_assignments[job_idx] = machines[:len(start_times)]
    
    # Determinar o número de máquinas
    machines_used = set(machine for job_assignments in job_machine_assignments for machine in job_assignments)
    num_machines = max(machines_used) + 1
    
    # Criar dicionário para mapear operações para máquinas
    machine_operations = {i: [] for i in range(num_machines)}
    for job_idx, (start_times, durations, machine_assignments) in enumerate(
            zip(jobs_start_times, job_durations, job_machine_assignments)):
        for op_idx, (start, duration, machine) in enumerate(
                zip(start_times, durations, machine_assignments)):
            if op_idx < len(machine_assignments):
                machine_operations[machine].append({
                    'job': job_idx,
                    'operation': op_idx,
                    'start': start,
                    'duration': duration
                })
    
    # Verificar sobreposição
    if not verify_no_overlap(machine_operations):
        print(f"AVISO: Detectada sobreposição no arquivo {jobs_file}!")
    else:
        print(f"Nenhuma sobreposição detectada no arquivo {jobs_file}.")
    
    # Definir cores distintas e mais contrastantes
    colors = ['#e6194b', '#3cb44b', '#0082c8', '#f58231', '#911eb4', '#46f0f0']
    if len(jobs_start_times) > len(colors):
        for _ in range(len(jobs_start_times) - len(colors)):
            hex_color = "#{:06x}".format(random.randint(0, 0xFFFFFF))
            colors.append(hex_color)
    
    # Criar figura com tamanho ajustado
    fig, ax = plt.subplots(figsize=(16, 8))
    
    # Altura das barras e espaçamento
    bar_height = 0.6
    y_spacing = 1.2
    
    # Desenhar as barras para cada máquina
    for machine_idx, operations in sorted(machine_operations.items()):
        y_position = machine_idx * y_spacing
        for op in operations:
            job_idx = op['job']
            op_idx = op['operation']
            start = op['start']
            duration = op['duration']
            
            # Desenhar a barra
            rect = patches.Rectangle(
                (start, y_position - bar_height/2),
                duration,
                bar_height,
                linewidth=1,
                edgecolor='black',
                facecolor=colors[job_idx % len(colors)],
                alpha=0.85
            )
            ax.add_patch(rect)
            
            # Adicionar texto com rotação e efeito de contorno
            label = f'j{job_idx},o{op_idx}'
            text = ax.text(
                start + duration/2,
                y_position,
                label,
                ha='center',
                va='center',
                fontsize=7,
                color='white',
                rotation=45,
                path_effects=[withStroke(linewidth=2, foreground='black')]
            )
            
            # Adicionar tempos de início e fim abaixo da barra
            if duration > 2:  # Mostrar tempos apenas para barras largas o suficiente
                ax.text(start, y_position - bar_height/2 - 0.1, f'{start}', ha='center', va='top', fontsize=6, color='black')
                ax.text(start + duration, y_position - bar_height/2 - 0.1, f'{start + duration}', ha='center', va='top', fontsize=6, color='black')
    
    # Configurar eixos
    ax.set_yticks([i * y_spacing for i in range(num_machines)])
    ax.set_yticklabels([f'Machine {i}' for i in range(num_machines)], fontsize=10)
    ax.set_xlim(0, makespan * 1.05)
    ax.set_ylim(-0.5, num_machines * y_spacing)
    
    # Adicionar grade
    ax.grid(True, which='both', linestyle='--', alpha=0.5)
    
    # Configurar rótulos e título
    ax.set_xlabel('Time', fontsize=12)
    ax.set_title(f'Job Shop Schedule - {jobs_file} (Makespan: {makespan})', fontsize=14, pad=20)
    
    # Adicionar legenda fora do gráfico
    handles = [patches.Patch(color=colors[j % len(colors)], label=f'Job {j}') 
               for j in range(len(jobs_start_times))]
    ax.legend(handles=handles, loc='center left', bbox_to_anchor=(1, 0.5), fontsize=10)
    
    # Ajustar layout
    plt.tight_layout()
    plt.subplots_adjust(right=0.85)  # Espaço para a legenda
    plt.savefig(output_image, dpi=200, bbox_inches='tight')
    plt.close()
    
    return makespan

def main():
    parser = argparse.ArgumentParser(description='Generate, solve, and visualize Job Shop Scheduling problems')
    parser.add_argument('--type', type=int, required=True, help='0 for small, 1 for medium, 2 for huge')
    parser.add_argument('--input_file', type=str, required=True, help='Input file for the Job Shop problem')
    parser.add_argument('--output_file', type=str, required=True, help='Output file for the Job Shop solution')

    args = parser.parse_args()

    # Fixed mapping files for each type
    if args.type == 0:
        print("Gerando problema de Job Shop Small file...")
        mapping_file = ".\\mappings\\mapping_small.txt"
    elif args.type == 1:
        print("Gerando problema de Job Shop Medium file...")
        mapping_file = ".\\mappings\\mapping_medium.txt"
    elif args.type == 2:
        print("Gerando problema de Job Shop Huge file...")
        mapping_file = ".\\mappings\\mapping_huge.txt"
    else:
        print("Tipo inválido. Use 0 para small, 1 para medium ou 2 para huge.")
        return

    # Use the input and output files provided in arguments
    input_file = args.input_file
    output_file = args.output_file
    
    # Create output image using the output_file name for the image
    output_image = f".\\viz\\gantt_{output_file.replace('.jss', '.png')}"
    create_job_shop_gantt(input_file, mapping_file, output_image)
    
    print("Gráfico gerado com sucesso!")

if __name__ == "__main__":
    main()
   