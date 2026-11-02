# Apply safe repository settings and branch protection to a public repo.
#
# Requires a GitHub token with administration permission for the repository.
# NEVER commit the token. Pass it through the environment:
#
#   $env:GITHUB_TOKEN = "ghp_..."
#   powershell -File tools\setup_github_protection.ps1 `
#       -Owner lferrete -Repo brucelee-vita -Branch main
#
# The script does not print the token.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Owner,
    [Parameter(Mandatory = $true)][string]$Repo,
    [string]$Branch = "main"
)

$ErrorActionPreference = "Stop"
if (-not $env:GITHUB_TOKEN) {
    throw "Set GITHUB_TOKEN in the environment (do not hardcode it)."
}

$headers = @{
    Authorization          = "Bearer $env:GITHUB_TOKEN"
    Accept                 = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
    "User-Agent"           = "brucelee-vita-protection"
}
$api = "https://api.github.com/repos/$Owner/$Repo"

function Invoke-GitHub($Method, $Uri, $Body = $null) {
    $params = @{
        Method  = $Method
        Uri     = $Uri
        Headers = $headers
    }
    if ($null -ne $Body) {
        $params.Body = ($Body | ConvertTo-Json -Depth 12)
        $params.ContentType = "application/json"
    }
    Invoke-RestMethod @params
}

Write-Host "== Repository settings =="
$repoSettings = @{
    has_issues                     = $true
    has_projects                   = $false
    has_wiki                       = $false
    has_discussions                = $false
    allow_squash_merge             = $true
    allow_merge_commit             = $false
    allow_rebase_merge             = $false
    delete_branch_on_merge         = $true
    allow_auto_merge               = $false
    web_commit_signoff_required    = $true
}
Invoke-GitHub -Method PATCH -Uri $api -Body $repoSettings | Out-Null
Write-Host "  issues on; wiki/projects/discussions off; squash-only; auto-delete branches"

Write-Host "== Branch protection: $Branch =="
$protection = @{
    required_status_checks          = $null
    enforce_admins                  = $false
    required_pull_request_reviews   = @{
        dismiss_stale_reviews           = $true
        require_code_owner_reviews      = $false
        required_approving_review_count = 1
        require_last_push_approval      = $true
    }
    # Personal repositories cannot use user/team restrictions.  The field must
    # be present as null; "only the owner can push" is enforced by having no
    # collaborators.  If collaborators are added later, they still need a PR.
    restrictions                    = $null
    required_conversation_resolution = $true
    required_linear_history          = $true
    allow_force_pushes               = $false
    allow_deletions                  = $false
    block_creations                  = $false
    lock_branch                      = $false
    allow_fork_syncing               = $false
}
Invoke-GitHub -Method PUT -Uri "$api/branches/$Branch/protection" -Body $protection | Out-Null
Write-Host "  PR required, 1 approval, linear history, no force/deletion"
Write-Host "  (personal repo: only the owner can push; do not add collaborators)"

Write-Host "== Actions permissions =="
try {
    Invoke-GitHub -Method PUT -Uri "$api/actions/permissions" -Body @{ enabled = $false } | Out-Null
    Write-Host "  Actions disabled"
} catch {
    Write-Host "  WARN: could not disable Actions ($($_.Exception.Message))"
}
try {
    Invoke-GitHub -Method PUT -Uri "$api/actions/permissions/workflow" -Body @{
        default_workflow_permissions     = "read"
        can_approve_pull_request_reviews = $false
    } | Out-Null
    Write-Host "  workflow token read-only; PR approval disabled"
} catch {
    Write-Host "  WARN: could not set workflow permissions ($($_.Exception.Message))"
}

Write-Host "== Release tag protection (v*) =="
try {
    Invoke-GitHub -Method POST -Uri "$api/rulesets" -Body @{
        name        = "Protect release tags"
        target      = "tag"
        enforcement = "active"
        conditions  = @{
            ref_name = @{
                include = @("refs/tags/v*")
                exclude = @()
            }
        }
        rules       = @(
            @{ type = "deletion" },
            @{ type = "non_fast_forward" },
            @{ type = "update" }
        )
    } | Out-Null
    Write-Host "  tag ruleset created"
} catch {
    Write-Host "  WARN: could not create tag ruleset ($($_.Exception.Message))"
    Write-Host "        Create it manually: Settings -> Rules -> New tag ruleset (v*)"
}

Write-Host ""
Write-Host "Done. Verify in Settings -> Branches/Rulesets."
