#!/usr/bin/env bash
#
# cambiar_ip.sh - Le pasas la abreviatura de un modulo y su IP, y actualiza
#                 todos los .config del repo que necesitan esa IP.
#
#   ./cambiar_ip.sh km 10.0.0.2
#   ./cambiar_ip.sh ks 10.0.0.3
#   ./cambiar_ip.sh km 10.0.0.2 ks 10.0.0.3 ms 10.0.0.4 swap 10.0.0.5
#   ./cambiar_ip.sh all 127.0.0.1     -> vuelve todo a local
#
# Abreviaturas: km (Kernel Memory), ks (Kernel Scheduler),
#               ms (Memory Stick), swap (SWAP), cpu (CPU), all (todos)

set -uo pipefail

# ---------------------------------------------------------------------------
# Que claves de los .config corresponden a cada modulo.
#
# OJO: en io/IO.config la clave se llama IO_IP pero guarda la IP del KERNEL
# SCHEDULER al que la IO se conecta (ver io/src/client/client.c), por eso va
# en ks y no en un modulo aparte.
#
# CPU no tiene IP propia en ningun config: es cliente del KM, del KS y de los
# Memory Sticks, y nadie se conecta a ella (el KS la registra por el socket
# entrante, ver nueva_cpu() en kernel_scheduler). IP_CPU queda mapeada por si
# algun dia agregan esa clave; hoy el script avisa que no encontro nada.
# ---------------------------------------------------------------------------
claves_del_modulo() {
    case "$1" in
        km)   echo "IP_KERNEL_MEMORY IP_KM" ;;
        ks)   echo "IP_KERNEL_SCHEDULER IO_IP" ;;
        ms)   echo "IP_MEMORY_STICK" ;;
        swap) echo "IP_SWAP" ;;
        cpu)  echo "IP_CPU" ;;
        all)  echo "IP_KERNEL_MEMORY IP_KM IP_KERNEL_SCHEDULER IO_IP IP_MEMORY_STICK IP_SWAP IP_CPU" ;;
        *)    return 1 ;;
    esac
}

uso() {
    cat <<EOF
Uso: $(basename "$0") <modulo> <ip> [<modulo> <ip> ...]

Modulos:
  km     Kernel Memory
  ks     Kernel Scheduler
  ms     Memory Stick
  swap   SWAP
  cpu    CPU
  all    todos a la vez

Ejemplos:
  $(basename "$0") km 10.0.0.2
  $(basename "$0") km 10.0.0.2 ks 10.0.0.3 ms 10.0.0.4 swap 10.0.0.5
  $(basename "$0") all 127.0.0.1
EOF
}

ip_valida() {
    local ip="$1" octeto
    [[ $ip =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]] || return 1
    for octeto in ${ip//./ }; do
        (( octeto > 255 )) && return 1
    done
    return 0
}

# ---------------------------------------------------------------------------
# Validacion de los argumentos
# ---------------------------------------------------------------------------
if (( $# == 0 )); then
    uso
    exit 1
fi

if (( $# % 2 != 0 )); then
    echo "error: los argumentos van de a pares <modulo> <ip>" >&2
    echo >&2
    uso >&2
    exit 1
fi

for (( i = 1; i <= $#; i += 2 )); do
    modulo="${!i}"
    j=$(( i + 1 ))
    ip="${!j}"

    if ! claves_del_modulo "$modulo" >/dev/null; then
        echo "error: modulo desconocido '$modulo' (usa: km, ks, ms, swap, all)" >&2
        exit 1
    fi

    if ! ip_valida "$ip"; then
        echo "error: '$ip' no es una IP valida" >&2
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# Raiz del repo: asi lo podes correr desde cualquier carpeta
# ---------------------------------------------------------------------------
raiz="$(git rev-parse --show-toplevel 2>/dev/null)" || raiz="$(pwd -P)"

configs=()
while IFS= read -r archivo; do
    configs+=("$archivo")
done < <(find "$raiz" -path '*/.git' -prune -o -type f -name '*.config' -print | sort)

if (( ${#configs[@]} == 0 )); then
    echo "error: no encontre ningun .config en $raiz" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# El cambio
# ---------------------------------------------------------------------------
total=0

for (( i = 1; i <= $#; i += 2 )); do
    modulo="${!i}"
    j=$(( i + 1 ))
    ip="${!j}"

    echo "[$modulo] -> $ip"

    presentes=0

    for clave in $(claves_del_modulo "$modulo"); do
        for archivo in "${configs[@]}"; do

            # IP que tiene ahora esa clave en este archivo (si es que la tiene)
            viejo="$(sed -nE "s|^[[:space:]]*${clave}[[:space:]]*=[[:space:]]*([^[:space:]#]+).*|\1|p" "$archivo" | head -1)"

            [[ -z $viejo ]] && continue
            presentes=$(( presentes + 1 ))
            [[ $viejo == "$ip" ]] && continue

            # Reemplazo solo el valor: quedan intactos los comentarios,
            # los espacios y el resto de las claves del archivo.
            sed -i -E "s|^([[:space:]]*${clave}[[:space:]]*=[[:space:]]*)[^[:space:]#]+|\1${ip}|" "$archivo"

            echo "    ${archivo#"$raiz"/}: $clave $viejo -> $ip"
            total=$(( total + 1 ))
        done
    done

    if (( presentes == 0 )); then
        echo "    aviso: ningun .config tiene una clave de IP para '$modulo', no cambie nada"
    fi
done

echo
if (( total == 0 )); then
    echo "Nada para cambiar, ya estaban con esas IPs."
else
    echo "Listo: $total valor(es) actualizados."
fi
