#!/usr/bin/env bash
# ==============================================================================
# play_match.sh - Lines of Action UCI Bot Match Coordinator
#
# Coordinates two UCI-compliant bots playing against each other via stdin/stdout.
# Assumes bots always make legal moves.
#
# Usage:
#   ./play_match.sh [bot1_path] [bot2_path] [max_plies] [--show-board]
#
# Defaults:
#   bot1: ./loabot (plays Black)
#   bot2: ./loabot (plays White)
#   max_plies: 40
# ==============================================================================

set -uo pipefail

BOT1="${1:-./loabot}"
BOT2="${2:-./loabot}"
MAX_PLIES="${3:-40}"
SHOW_BOARD=false

for arg in "$@"; do
    if [[ "$arg" == "--show-board" ]] || [[ "$arg" == "-v" ]]; then
        SHOW_BOARD=true
    fi
done

# Ensure bots exist and are executable
if [[ ! -x "$BOT1" ]]; then
    echo "Error: Bot 1 executable '$BOT1' not found or not executable." >&2
    exit 1
fi
if [[ ! -x "$BOT2" ]]; then
    echo "Error: Bot 2 executable '$BOT2' not found or not executable." >&2
    exit 1
fi

echo "=================================================="
echo "          Lines of Action - Bot Match             "
echo "=================================================="
echo "Black (Player 1) : $BOT1"
echo "White (Player 2) : $BOT2"
echo "Max Plies        : $MAX_PLIES"
echo "Show Board       : $SHOW_BOARD"
echo "=================================================="

# Create temporary directory for FIFOs
TMPDIR=$(mktemp -d /tmp/loa_match_XXXXXX)

cleanup() {
    # Send quit to both bots if still alive
    if [[ -n "${PID1:-}" ]] && kill -0 "$PID1" 2>/dev/null; then
        echo "quit" >&3 2>/dev/null || true
        wait "$PID1" 2>/dev/null || true
    fi
    if [[ -n "${PID2:-}" ]] && kill -0 "$PID2" 2>/dev/null; then
        echo "quit" >&5 2>/dev/null || true
        wait "$PID2" 2>/dev/null || true
    fi

    # Close file descriptors
    exec 3>&- 4<&- 5>&- 6<&- 2>/dev/null || true
    rm -rf "$TMPDIR"
}

trap cleanup EXIT INT TERM

# Create named pipes for IPC
mkfifo "$TMPDIR/b1_in" "$TMPDIR/b1_out" "$TMPDIR/b2_in" "$TMPDIR/b2_out"

# Launch Bot 1 (Black)
"$BOT1" < "$TMPDIR/b1_in" > "$TMPDIR/b1_out" 2>/dev/null &
PID1=$!
exec 3> "$TMPDIR/b1_in"
exec 4< "$TMPDIR/b1_out"

# Launch Bot 2 (White)
"$BOT2" < "$TMPDIR/b2_in" > "$TMPDIR/b2_out" 2>/dev/null &
PID2=$!
exec 5> "$TMPDIR/b2_in"
exec 6< "$TMPDIR/b2_out"

# ------------------------------------------------------------------------------
# UCI Handshake Helpers
# ------------------------------------------------------------------------------
perform_handshake() {
    local bot_name="$1"
    local fd_in="$2"
    local fd_out="$3"

    echo "uci" >&"$fd_in"
    local engine_id="$bot_name"
    while IFS= read -r line <&"$fd_out"; do
        if [[ "$line" =~ ^id\ name\ (.*) ]]; then
            engine_id="${BASH_REMATCH[1]}"
        fi
        if [[ "$line" == "uciok" ]]; then
            break
        fi
    done

    echo "isready" >&"$fd_in"
    while IFS= read -r line <&"$fd_out"; do
        if [[ "$line" == "readyok" ]]; then
            break
        fi
    done

    echo "ucinewgame" >&"$fd_in"
    echo "  [Handshake OK] $bot_name identified as: $engine_id"
}

echo "Initiating handshake..."
perform_handshake "Bot 1 (Black)" 3 4
perform_handshake "Bot 2 (White)" 5 6
echo "Handshake complete. Starting game!"
echo "--------------------------------------------------"

# ------------------------------------------------------------------------------
# Game Loop
# ------------------------------------------------------------------------------
MOVES=""
PLY=1

while (( PLY <= MAX_PLIES )); do
    MOVE_NUM=$(( (PLY + 1) / 2 ))

    if (( PLY % 2 == 1 )); then
        PLAYER="Black"
        FD_IN=3
        FD_OUT=4
    else
        PLAYER="White"
        FD_IN=5
        FD_OUT=6
    fi

    # Prepare position command
    if [[ -z "$MOVES" ]]; then
        CMD="position startpos"
    else
        CMD="position startpos moves$MOVES"
    fi

    echo "$CMD" >&"$FD_IN"

    # Optional: Display board state before move
    if [[ "$SHOW_BOARD" == "true" ]]; then
        echo "d" >&"$FD_IN"
        while IFS= read -r line <&"$FD_OUT"; do
            echo "    $line"
            if [[ "$line" =~ ^Side\ to\ move: ]]; then
                break
            fi
        done
    fi

    # Request move from current bot
    echo "go" >&"$FD_IN"

    BESTMOVE=""
    while IFS= read -r line <&"$FD_OUT"; do
        if [[ "$line" =~ ^bestmove\ ([^ ]+) ]]; then
            BESTMOVE="${BASH_REMATCH[1]}"
            break
        fi
    done

    if [[ -z "$BESTMOVE" ]] || [[ "$BESTMOVE" == "(none)" ]] || [[ "$BESTMOVE" == "none" ]]; then
        echo "Move $MOVE_NUM ($PLAYER): No move returned or bot resigned. Ending game."
        break
    fi

    printf "Move %2d. %-5s played: %s\n" "$MOVE_NUM" "$PLAYER" "$BESTMOVE"
    MOVES="$MOVES $BESTMOVE"

    PLY=$((PLY + 1))
done

echo "--------------------------------------------------"
echo "Match finished."
echo "Total plies: $(( PLY - 1 ))"
echo "Moves record:"
if [[ -n "$MOVES" ]]; then
    echo "position startpos moves$MOVES"
else
    echo "(No moves were played)"
fi
echo "=================================================="
