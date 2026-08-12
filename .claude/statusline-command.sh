#!/bin/bash
# Claude Code status line command
# Comprehensive display: directory, repo, model, context, PR, rate limits, etc.

input=$(cat)

# --- Extract all available fields ---

# Directory
dir=$(echo "$input" | jq -r '.workspace.current_dir // empty')
dir_short=""
[ -n "$dir" ] && dir_short=$(basename "$dir")

# Project
project=$(echo "$input" | jq -r '.workspace.project_dir // empty')
project_short=""
[ -n "$project" ] && project_short=$(basename "$project")

# Repository
repo=$(echo "$input" | jq -r '.workspace.repo | if . then .owner + "/" + .name else empty end')

# Model
model=$(echo "$input" | jq -r '.model.display_name // empty')

# Session name
session=$(echo "$input" | jq -r '.session_name // empty')

# Agent
agent=$(echo "$input" | jq -r '.agent.name // empty')

# Output style
style=$(echo "$input" | jq -r '.output_style.name // empty')

# Effort
effort=$(echo "$input" | jq -r '.effort.level // empty')

# Context
used=$(echo "$input" | jq -r '.context_window.used_percentage // empty')

# PR
pr_num=$(echo "$input" | jq -r '.pr.number // empty')
pr_state=$(echo "$input" | jq -r '.pr.review_state // empty')

# Rate limits
five_h=$(echo "$input" | jq -r '.rate_limits.five_hour.used_percentage // empty')
seven_d=$(echo "$input" | jq -r '.rate_limits.seven_day.used_percentage // empty')

# Vim mode
vim_mode=$(echo "$input" | jq -r '.vim.mode // empty')

# Workspace name (for worktrees)
worktree=$(echo "$input" | jq -r '.worktree.name // empty')

# --- Build status line ---
out=""

# Section 1: Location
if [ -n "$dir_short" ]; then
    out="$dir_short"
fi
if [ -n "$repo" ]; then
    if [ -n "$out" ]; then
        out="$out | $repo"
    else
        out="$repo"
    fi
elif [ -n "$project_short" ]; then
    if [ -n "$out" ]; then
        out="$out | $project_short"
    else
        out="$project_short"
    fi
fi

# Section 2: Session / Agent
if [ -n "$session" ]; then
    out="$out | $session"
fi
if [ -n "$agent" ]; then
    out="$out | $agent"
fi

# Section 3: Model
if [ -n "$model" ]; then
    out="$out | $model"
fi

# Section 4: Effort
if [ -n "$effort" ]; then
    out="$out | effort: $effort"
fi

# Section 5: Style (only show if non-default)
if [ -n "$style" ] && [ "$style" != "default" ]; then
    out="$out | $style"
fi

# Section 6: Context
if [ -n "$used" ]; then
    out="$out | ctx: ${used}%"
fi

# Section 7: PR
if [ -n "$pr_num" ]; then
    pr_text="PR #$pr_num"
    case "$pr_state" in
        "approved") pr_text="$pr_text (approved)" ;;
        "changes_requested") pr_text="$pr_text (changes)" ;;
        "pending") pr_text="$pr_text (pending)" ;;
        "draft") pr_text="$pr_text (draft)" ;;
    esac
    out="$out | $pr_text"
fi

# Section 8: Rate limits
rate_parts=""
if [ -n "$five_h" ]; then
    rate_parts="5h:$(printf '%.0f' "$five_h")%"
fi
if [ -n "$seven_d" ]; then
    if [ -n "$rate_parts" ]; then
        rate_parts="$rate_parts 7d:$(printf '%.0f' "$seven_d")%"
    else
        rate_parts="7d:$(printf '%.0f' "$seven_d")%"
    fi
fi
if [ -n "$rate_parts" ]; then
    out="$out | $rate_parts"
fi

# Section 9: Vim mode
if [ -n "$vim_mode" ]; then
    out="$out | vim: $vim_mode"
fi

# Section 10: Worktree
if [ -n "$worktree" ]; then
    out="$out | worktree: $worktree"
fi

# If nothing was populated, show a minimal fallback
if [ -z "$out" ]; then
    out="Claude Code"
fi

echo "$out"
