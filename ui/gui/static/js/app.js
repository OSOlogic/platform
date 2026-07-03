/**
 * @file app.js
 * @author Diego Arcos Sapena
 * @brief PLC OsoLogic GUI (Web Interface)
 * @version a-1.0.0
 * @date 2024/11/23
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */


// =============================================================================
// STATE
// =============================================================================

let currentPage = 'rtmirror';
let models = [];
let devices = [];
let currentUser = {
    username: null,
    role: null,
    user_id: null
};

let guiConfig = {
    decimalSeparator: localStorage.getItem('decimalSeparator') || '.'
};

// =============================================================================
// INITIALIZATION
// =============================================================================

document.addEventListener('DOMContentLoaded', async () => {
    await loadCurrentUser();
    initNavigation();
    initMobileMenu();
    loadPage('rtmirror');
});

async function loadCurrentUser() {
    try {
        const response = await fetch('/api/auth/me');
        if (response.ok) {
            currentUser = await response.json();
            updateUIForRole();
        }
    } catch (error) {
        console.error('Failed to load user info:', error);
    }
}

function updateUIForRole() {
    // Update user info display
    const userNameEl = document.getElementById('userName');
    const userRoleEl = document.getElementById('userRole');
    
    if (userNameEl) userNameEl.textContent = currentUser.username || 'Unknown';
    if (userRoleEl) {
        userRoleEl.textContent = currentUser.role || 'viewer';
        userRoleEl.className = 'user-role badge';
        if (currentUser.role === 'admin') {
            userRoleEl.classList.add('badge-danger');
        } else if (currentUser.role === 'operator') {
            userRoleEl.classList.add('badge-success');
        } else {
            userRoleEl.classList.add('badge-info');
        }
    }
    
    // Show/hide admin section
    const adminSection = document.getElementById('adminSection');
    if (adminSection) {
        adminSection.style.display = currentUser.role === 'admin' ? 'block' : 'none';
    }
}

function canWrite() {
    return currentUser.role === 'admin' || currentUser.role === 'operator';
}

function isAdmin() {
    return currentUser.role === 'admin';
}

/**
 * Validate a numeric input from a form element.
 * @param {string|number} val - The raw value to validate
 * @param {string} label - Human-readable field label for error messages
 * @param {object} opts - options: {required, min, max, integer, choices}
 * @returns {{valid: boolean, value: number|null, error: string|null}}
 */
function validateInt(val, label, opts = {}) {
    if (val === '' || val == null) {
        if (opts.required) return {valid: false, value: null, error: `${label} is required`};
        return {valid: true, value: null, error: null};
    }
    const num = opts.integer !== false ? parseInt(val) : parseDecimal(val);
    if (isNaN(num)) return {valid: false, value: null, error: `${label} must be a valid number`};
    if (opts.min != null && num < opts.min) return {valid: false, value: null, error: `${label} must be at least ${opts.min}`};
    if (opts.max != null && num > opts.max) return {valid: false, value: null, error: `${label} must be at most ${opts.max}`};
    if (opts.choices && !opts.choices.includes(num)) return {valid: false, value: null, error: `${label} must be one of: ${opts.choices.join(', ')}`};
    return {valid: true, value: num, error: null};
}

/**
 * Filter input to allow only digits (0-9).
 * Use in onkeydown="onlyNumbers(event)"
 */
function onlyNumbers(e) {
    if (e.ctrlKey || e.altKey || e.metaKey) return;
    const key = e.key;
    // Allow navigation and control keys
    if (['Backspace', 'Delete', 'ArrowLeft', 'ArrowRight', 'Tab', 'Enter', 'Home', 'End'].includes(key)) return;
    // Prevent non-digit characters
    if (!/^[0-9]$/.test(key)) {
        e.preventDefault();
    }
}

/**
 * Filter input to allow digits (0-9), a single dot (.), and a minus sign (-).
 * Adjusted for GUI Config decimal separator.
 */
function onlyNumbersDotMinus(e) {
    if (e.ctrlKey || e.altKey || e.metaKey) return;
    const key = e.key;
    // Allow navigation and control keys
    if (['Backspace', 'Delete', 'ArrowLeft', 'ArrowRight', 'Tab', 'Enter', 'Home', 'End'].includes(key)) return;
    
    const separator = guiConfig.decimalSeparator;
    // Check for allowed characters
    if (!/^[0-9.-]$/.test(key) && key !== separator) {
        e.preventDefault();
        return;
    }
    
    // Prevent multiple separators
    if (key === separator && e.target.value.includes(separator)) {
        e.preventDefault();
        return;
    }

    // Special case: if standard dot is NOT the separator, prevent it
    if (key === '.' && separator !== '.') {
        e.preventDefault();
        return;
    }
    
    // Prevent multiple minus signs
    if (key === '-' && e.target.value.includes('-')) {
        e.preventDefault();
        return;
    }
}

function formatDecimal(val) {
    if (val === null || val === undefined || val === '') return '';
    let s = String(val);
    if (guiConfig.decimalSeparator === ',') {
        return s.replace('.', ',');
    }
    return s;
}

function parseDecimal(val) {
    if (val === '' || val == null) return null;
    let s = String(val);
    if (guiConfig.decimalSeparator === ',') {
        s = s.replace(',', '.');
    }
    // Remove any remaining invalid chars just in case
    s = s.replace(/[^0-9.-]/g, '');
    const n = parseFloat(s);
    return isNaN(n) ? null : n;
}

function initNavigation() {
    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', (e) => {
            e.preventDefault();
            const page = item.dataset.page;
            loadPage(page);
        });
    });
}

function initMobileMenu() {
    const toggle = document.getElementById('menuToggle');
    const sidebar = document.getElementById('sidebar');
    
    toggle.addEventListener('click', () => {
        sidebar.classList.toggle('open');
    });
}

// =============================================================================
// PAGE LOADING
// =============================================================================

function loadPage(page) {
    currentPage = page;
    
    // Update navigation
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.toggle('active', item.dataset.page === page);
    });
    
    // Close mobile menu
    document.getElementById('sidebar').classList.remove('open');
    
    // Load content
    const content = document.getElementById('contentArea');
    const pageTitle = document.getElementById('pageTitle');
    const headerActions = document.getElementById('headerActions');
    
    content.innerHTML = '<div class="loading"><div class="spinner"></div>Loading...</div>';
    headerActions.innerHTML = '';
    
    switch(page) {
        case 'dashboard':
            pageTitle.textContent = 'Dashboard';
            loadDashboard();
            break;
        case 'models':
            pageTitle.textContent = 'Models';
            if (canWrite()) {
                headerActions.innerHTML = '<button class="btn btn-primary" onclick="showModelWizard()">➕ New Model</button>';
            }
            loadModelsPage();
            break;
        case 'devices':
            pageTitle.textContent = 'Devices';
            headerActions.innerHTML = '<button class="btn btn-primary" onclick="showDeviceWizard()">➕ Add Device</button>';
            loadDevices();
            break;
        case 'io-definitions':
            pageTitle.textContent = 'I/O Definitions';
            headerActions.innerHTML = '<button class="btn btn-primary" onclick="showCreateIODefForm()">➕ Add Definition</button>';
            loadIODefinitions();
            break;
        case 'module-io-config':
            pageTitle.textContent = 'Module I/O Config';
            loadModuleIOConfig();
            break;
        case 'secure-state':
            pageTitle.textContent = 'Secure State Mapping';
            headerActions.innerHTML = '<button class="btn btn-primary" onclick="showCreateSecureStateForm()">➕ Add Mapping</button>';
            loadSecureStateMapping();
            break;
        case 'aggregated-io':
            pageTitle.textContent = 'Aggregated I/O Mapping';
            headerActions.innerHTML = '<button class="btn btn-primary" onclick="showCreateAggregatedIOForm()">➕ Add Mapping</button>';
            loadAggregatedIOMap();
            break;
        case 'plc-settings':
            pageTitle.textContent = 'PLC Config';
            loadPLCSettings();
            break;
        case 'rtmirror':
            pageTitle.textContent = 'Real-Time Twin';
            loadRTMirror();
            break;
        case 'gui-config':
            pageTitle.textContent = 'GUI Settings';
            loadGUIConfig();
            break;
        case 'users':
            pageTitle.textContent = 'User Management';
            if (isAdmin()) {
                headerActions.innerHTML = '<button class="btn btn-primary" onclick="showCreateUserForm()">➕ Add User</button>';
            }
            loadUsers();
            break;
    }
}

// =============================================================================
// DASHBOARD
// =============================================================================

async function loadDashboard() {
    try {
        const response = await fetch('/api/dashboard');
        const stats = await response.json();
        
        const content = document.getElementById('contentArea');
        content.innerHTML = `
            <div class="dashboard-grid">
                <div class="stat-card">
                    <div class="stat-icon">🔧</div>
                    <div class="stat-value">${stats.models}</div>
                    <div class="stat-label">Models</div>
                </div>
                <div class="stat-card">
                    <div class="stat-icon">📦</div>
                    <div class="stat-value">${stats.devices}</div>
                    <div class="stat-label">Devices</div>
                </div>
                <div class="stat-card">
                    <div class="stat-icon">🟢</div>
                    <div class="stat-value">${stats.connected_devices}</div>
                    <div class="stat-label">Connected</div>
                </div>
                <div class="stat-card">
                    <div class="stat-icon">📍</div>
                    <div class="stat-value">${stats.io_definitions}</div>
                    <div class="stat-label">I/O Points</div>
                </div>
                <div class="stat-card">
                    <div class="stat-icon">🔒</div>
                    <div class="stat-value">${stats.secure_state_mappings}</div>
                    <div class="stat-label">Secure Mappings</div>
                </div>
                <div class="stat-card">
                    <div class="stat-icon">🔀</div>
                    <div class="stat-value">${stats.aggregated_mappings}</div>
                    <div class="stat-label">Aggregated Mappings</div>
                </div>
            </div>
            
            <div class="card">
                <div class="card-title">⚡ System Status</div>
                <p>Operation Mode: <span class="badge ${stats.operation_mode === 'execution' ? 'badge-success' : 'badge-warning'}">${stats.operation_mode.toUpperCase()}</span></p>
            </div>
        `;
    } catch (error) {
        showToast('Failed to load dashboard', 'error');
    }
}

// =============================================================================
// MODELS — Browse Page with Expandable Cards
// =============================================================================

// Wizard state for multi-step create/edit
let wizardState = {
    mode: null,          // 'create' or 'edit'
    editId: null,
    step: 1,
    data: {},            // Step 1 fields
    ioPoints: [],        // Step 2 rows
    secureStateMappings: [] // Step 3 mappings
};

async function loadModelsPage() {
    try {
        const response = await fetch('/api/models');
        models = await response.json();

        const content = document.getElementById('contentArea');

        if (models.length === 0) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📦</div>
                    <p>No models defined yet</p>
                    ${canWrite() ? '<button class="btn btn-primary" onclick="showModelWizard()">➕ Create your first model</button>' : ''}
                </div>
            `;
            return;
        }

        content.innerHTML = `
            <div class="models-list">
                ${models.map(m => `
                    <div class="model-card" id="model-card-${m.model_id}">
                        <div class="model-card-header" onclick="toggleModelCard(${m.model_id})">
                            <div class="model-card-header-left">
                                <span class="model-expand-icon" id="expand-icon-${m.model_id}">▶</span>
                                <div>
                                    <strong>${m.model_name}</strong>
                                    <span class="badge badge-info">${m.protocol || '—'}</span>
                                </div>
                            </div>
                            <div class="model-card-header-stats">
                                <div class="model-detail-item">
                                    <span class="model-detail-label">Default Timeout</span>
                                    <span class="model-detail-value">${m.default_timeout_ms != null ? m.default_timeout_ms + ' ms' : 'N/A'}</span>
                                </div>
                                <div class="model-detail-item">
                                    <span class="model-detail-label">Max Read Bit Block</span>
                                    <span class="model-detail-value">${m.max_read_bit_block_size}</span>
                                </div>
                                <div class="model-detail-item">
                                    <span class="model-detail-label">Max Read Register Block</span>
                                    <span class="model-detail-value">${m.max_read_register_block_size}</span>
                                </div>
                                <div class="model-detail-item">
                                    <span class="model-detail-label">Max Write Bit Block</span>
                                    <span class="model-detail-value">${m.max_write_bit_block_size}</span>
                                </div>
                                <div class="model-detail-item">
                                    <span class="model-detail-label">Max Write Register Block</span>
                                    <span class="model-detail-value">${m.max_write_register_block_size}</span>
                                </div>
                            </div>
                            <div class="model-card-header-right">
                                ${canWrite() ? `
                                    <button class="btn btn-secondary btn-sm btn-icon" onclick="event.stopPropagation(); editModelWizard(${m.model_id})" title="Edit">✏️</button>
                                    <button class="btn btn-danger btn-sm btn-icon" onclick="event.stopPropagation(); deleteModelWithConfirm(${m.model_id}, '${m.model_name.replace(/'/g, "\\'")}')" title="Delete">🗑️</button>
                                ` : ''}
                            </div>
                        </div>
                        <div class="model-card-body" id="model-body-${m.model_id}" style="display:none;">
                            <div class="loading"><div class="spinner"></div>Loading I/O points...</div>
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    } catch (error) {
        showToast('Failed to load models', 'error');
    }
}

async function toggleModelCard(modelId) {
    const body = document.getElementById(`model-body-${modelId}`);
    const icon = document.getElementById(`expand-icon-${modelId}`);

    if (body.style.display === 'none') {
        body.style.display = 'block';
        icon.textContent = '▼';
        // Load full model data
        try {
            const response = await fetch(`/api/models/${modelId}/full`);
            const model = await response.json();

            const ioDefs = model.io_definitions || [];
            const ssm = model.secure_state_mappings || [];

            // Build lookup: secure_io_def_id -> standard label
            const ssmLookup = {};
            ssm.forEach(m => {
                ssmLookup[m.fk_secure_state_io_definition_id] = {
                    standard_label: m.standard_label,
                    standard_address: m.standard_address,
                    standard_io_type: m.standard_io_type
                };
            });

            // Group by purpose
            const grouped = { standard: [], secure_state: [], config: [] };
            ioDefs.forEach(d => {
                const p = d.purpose || 'standard';
                if (!grouped[p]) grouped[p] = [];
                grouped[p].push(d);
            });

            let html = '';
            for (const [purpose, defs] of Object.entries(grouped)) {
                if (defs.length === 0) continue;
                const purposeLabel = purpose.replace('_', ' ').replace(/\b\w/g, c => c.toUpperCase());
                const purposeColor = purpose === 'standard' ? 'badge-success' : purpose === 'secure_state' ? 'badge-warning' : 'badge-info';

                html += `
                    <div class="io-group">
                        <h4><span class="badge ${purposeColor}">${purposeLabel}</span> <span class="io-count">${defs.length} points</span></h4>
                        <div class="table-container">
                            <table>
                                <thead>
                                    <tr>
                                        <th>Logical Addr</th>
                                        <th>Type</th>
                                        <th>Access</th>
                                        <th>Physical Addr</th>
                                        <th>Method</th>
                                        <th>Regs</th>
                                        <th>Endian</th>
                                        <th>Default Label</th>
                                        ${purpose === 'secure_state' ? '<th>↔ Standard Ref</th>' : ''}
                                    </tr>
                                </thead>
                                <tbody>
                                    ${defs.map(d => {
                                        const type = String(d.io_type || '').trim().toLowerCase();
                                        const access = String(d.hardware_access || '').trim().toLowerCase();
                                        const typeBadge = type === 'bit' ? 'badge-info' : 'badge-warning';
                                        const accessBadge = access === 'readonly' ? 'badge-readonly' : 'badge-readwrite';
                                        const ref = ssmLookup[d.io_definition_id];
                                        return `
                                            <tr>
                                                <td>${d.logical_address}</td>
                                                <td><span class="badge ${typeBadge}">${type}</span></td>
                                                <td><span class="badge ${accessBadge}">${access}</span></td>
                                                <td>${d.physical_address != null ? d.physical_address : '—'}</td>
                                                <td>${d.access_method}${d.bitmask_offset != null ? ':' + d.bitmask_offset : ''}</td>
                                                <td>${d.register_count}</td>
                                                <td>${d.endianess}</td>
                                                <td>${d.default_io_label || '—'}</td>
                                                ${purpose === 'secure_state' ? `<td>${ref ? `↔ [${ref.standard_io_type}@${ref.standard_address}] ${ref.standard_label || ''}` : '<em>unmapped</em>'}</td>` : ''}
                                            </tr>
                                        `;
                                    }).join('')}
                                </tbody>
                            </table>
                        </div>
                    </div>
                `;
            }

            if (ioDefs.length === 0) {
                html = '<div class="empty-state" style="padding: 20px;"><p>No I/O points defined for this model</p></div>';
            }

            // Show aggregated children (if any)
            const aggChildren = model.aggregated_children || [];
            if (aggChildren.length > 0) {
                html += `
                    <div class="io-group">
                        <h4><span class="badge badge-accent">🧩 Child Models</span> <span class="io-count">${aggChildren.length} slots</span></h4>
                        <div class="table-container">
                            <table>
                                <thead>
                                    <tr>
                                        <th style="width: 60px;">Slot</th>
                                        <th>Child Model</th>
                                        <th>Model ID</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    ${aggChildren.map(c => `
                                        <tr>
                                            <td><strong>${c.slot_index}</strong></td>
                                            <td>${c.child_model_name}</td>
                                            <td>${c.fk_child_model_id}</td>
                                        </tr>
                                    `).join('')}
                                </tbody>
                            </table>
                        </div>
                    </div>
                `;
            }

            body.innerHTML = html;
        } catch (error) {
            body.innerHTML = '<p style="color: var(--danger); padding: 16px;">Failed to load model details</p>';
        }
    } else {
        body.style.display = 'none';
        icon.textContent = '▶';
    }
}

// =============================================================================
// MODELS — Delete
// =============================================================================

async function deleteModelWithConfirm(id, name) {
    if (!confirm(`Are you sure you want to delete model "${name}"?\n\nThis will also delete ALL associated I/O definitions and secure state mappings.`)) {
        return;
    }

    try {
        const response = await fetch(`/api/models/${id}`, { method: 'DELETE' });

        if (response.ok) {
            showToast('Model deleted successfully', 'success');
            loadModelsPage();
        } else {
            const result = await response.json();
            showToast(result.error || 'Failed to delete model', 'error');
        }
    } catch (error) {
        showToast('Failed to delete model', 'error');
    }
}

// =============================================================================
// MODELS — 3-Step Wizard (Create / Edit)
// =============================================================================

function showModelWizard() {
    wizardState = {
        mode: 'create',
        editId: null,
        step: 1,
        data: {
            model_name: '',
            protocol: 'modbus-rtu',
            default_timeout_ms: 1000,
            max_read_bit_block_size: 16,
            max_read_register_block_size: 16,
            max_write_bit_block_size: 16,
            max_write_register_block_size: 16
        },
        ioPoints: [],
        secureStateMappings: [],
        aggregatedChildren: []  // [{fk_child_model_id: X}, ...]
    };
    // Hide header action button while in wizard
    document.getElementById('headerActions').innerHTML = '';
    document.getElementById('pageTitle').textContent = 'New Model';
    renderWizardStep();
}

async function editModelWizard(id) {
    try {
        const response = await fetch(`/api/models/${id}/full`);
        const model = await response.json();

        wizardState = {
            mode: 'edit',
            editId: id,
            step: 1,
            data: {
                model_name: model.model_name,
                protocol: model.protocol || 'modbus-rtu',
                default_timeout_ms: model.default_timeout_ms,
                max_read_bit_block_size: model.max_read_bit_block_size,
                max_read_register_block_size: model.max_read_register_block_size,
                max_write_bit_block_size: model.max_write_bit_block_size,
                max_write_register_block_size: model.max_write_register_block_size
            },
            ioPoints: (model.io_definitions || []).map(d => {
                const pt = {
                    io_definition_id: d.io_definition_id,
                    logical_address: d.logical_address,
                    io_type: d.io_type,
                    purpose: d.purpose,
                    hardware_access: d.hardware_access,
                    physical_address: d.physical_address,
                    access_method: d.access_method || 'direct',
                    bitmask_offset: d.bitmask_offset,
                    register_count: d.register_count || 1,
                    endianess: d.endianess || 'big',
                    default_io_label: d.default_io_label || ''
                };
                
                // Map aggregated IO point back to child using aggregated_io_map
                if (model.aggregated_io_map) {
                    const mapping = model.aggregated_io_map.find(m => m.fk_aggregated_io_definition_id === d.io_definition_id);
                    if (mapping) {
                        pt._childSlotIndex = mapping.child_slot_index;
                        pt._childIoDefId = mapping.fk_child_io_definition_id;
                    }
                }
                return pt;
            }),
            secureStateMappings: (model.secure_state_mappings || []).map(m => ({
                fk_secure_state_io_definition_id: m.fk_secure_state_io_definition_id,
                fk_standard_io_definition_id: m.fk_standard_io_definition_id
            })),
            aggregatedChildren: (model.aggregated_children || []).map(c => ({
                fk_child_model_id: c.fk_child_model_id
            })),
            // Smart Update: Remember original IO definition IDs to compute diff on save
            _originalIoIds: (model.io_definitions || []).map(d => d.io_definition_id)
        };
        // Store initial state to detect changes
        wizardState.initialAggregatedChildrenJson = JSON.stringify(wizardState.aggregatedChildren);

        document.getElementById('headerActions').innerHTML = '';
        document.getElementById('pageTitle').textContent = `Edit Model — ${model.model_name}`;
        renderWizardStep();
    } catch (error) {
        showToast('Failed to load model for editing', 'error');
    }
}

function isAggregatedModel() {
    return wizardState.data.protocol === 'aggregated';
}

async function renderWizardStep() {
    const content = document.getElementById('contentArea');
    const step = wizardState.step;
    const isAgg = isAggregatedModel();
    // Step labels: for aggregated, Child Models comes before I/O Points
    const stepLabels = isAgg
        ? ['General Config', 'Child Models', 'I/O Points', 'Secure State Mapping']
        : ['General Config', 'I/O Points', 'Secure State Mapping'];
    const stepsToShow = stepLabels.length;

    let stepperHtml = '<div class="wizard-stepper">';
    for (let i = 1; i <= stepsToShow; i++) {
        const cls = i === step ? 'active' : (i < step ? 'completed' : '');
        stepperHtml += `
            <div class="wizard-step ${cls}">
                <div class="wizard-step-circle">${i < step ? '✓' : i}</div>
                <div class="wizard-step-label">${stepLabels[i - 1]}</div>
            </div>
            ${i < stepsToShow ? '<div class="wizard-step-line ' + (i < step ? 'completed' : '') + '"></div>' : ''}
        `;
    }
    stepperHtml += '</div>';

    let bodyHtml = '';
    if (isAgg) {
        // Aggregated order: General → Child Models → I/O → Secure State
        switch (step) {
            case 1: bodyHtml = renderWizardStep1(); break;
            case 2: bodyHtml = await renderWizardStep4(); break;  // Child Models
            case 3: bodyHtml = renderWizardStep2(); break;        // I/O Points
            case 4: bodyHtml = renderWizardStep3(); break;        // Secure State
        }
    } else {
        switch (step) {
            case 1: bodyHtml = renderWizardStep1(); break;
            case 2: bodyHtml = renderWizardStep2(); break;
            case 3: bodyHtml = renderWizardStep3(); break;
        }
    }

    content.innerHTML = stepperHtml + '<div class="wizard-body">' + bodyHtml + '</div>';

    // Attach event handlers matching the correct render function for each step
    if (step === 1) attachStep1Handlers();
    if (isAgg) {
        if (step === 2) attachStep4Handlers();  // Child Models handlers
        if (step === 3) attachStep2Handlers();  // I/O Points handlers
        if (step === 4) attachStep3Handlers();  // Secure State handlers
    } else {
        if (step === 2) attachStep2Handlers();
        if (step === 3) attachStep3Handlers();
    }
}

// --- Step 1: General Configuration ---

function renderWizardStep1() {
    const d = wizardState.data;
    return `
        <div class="card">
            <div class="card-title">📋 General Configuration</div>
            <form id="wizardStep1Form">
                <div class="form-row">
                    <div class="form-group" style="flex:2">
                        <label for="wiz_model_name">Model Name *</label>
                        <input type="text" id="wiz_model_name" class="form-control" required placeholder="e.g., Borrell 8SD" value="${d.model_name}">
                    </div>
                    <div class="form-group" style="flex:1">
                        <label for="wiz_protocol">Interface Protocol *</label>
                        <select id="wiz_protocol" class="form-control" required>
                            <option value="borrell-spi" ${d.protocol === 'borrell-spi' ? 'selected' : ''}>Borrell SPI</option>
                            <option value="modbus-rtu" ${d.protocol === 'modbus-rtu' ? 'selected' : ''}>Modbus RTU</option>
                            <option value="modbus-tcp" ${d.protocol === 'modbus-tcp' ? 'selected' : ''}>Modbus TCP</option>
                            <option value="aggregated" ${d.protocol === 'aggregated' ? 'selected' : ''}>Aggregated</option>
                        </select>
                    </div>
                </div>
                <div class="form-group">
                    <label for="wiz_default_timeout_ms">Default Timeout (ms)</label>
                    <input type="number" id="wiz_default_timeout_ms" class="form-control" value="${d.default_timeout_ms != null ? d.default_timeout_ms : ''}" placeholder="e.g., 1000" onkeydown="onlyNumbers(event)">
                    <span class="form-hint">Leave empty if not applicable (e.g., SPI)</span>
                </div>
                    <div class="form-group">
                        <label for="wiz_max_read_bit_block_size">Max Read Bit Block</label>
                        <input type="number" id="wiz_max_read_bit_block_size" class="form-control" value="${d.max_read_bit_block_size}" min="0" onkeydown="onlyNumbers(event)">
                    </div>
                    <div class="form-group">
                        <label for="wiz_max_read_register_block_size">Max Read Reg Block</label>
                        <input type="number" id="wiz_max_read_register_block_size" class="form-control" value="${d.max_read_register_block_size}" min="0" onkeydown="onlyNumbers(event)">
                    </div>
                    <div class="form-group">
                        <label for="wiz_max_write_bit_block_size">Max Write Bit Block</label>
                        <input type="number" id="wiz_max_write_bit_block_size" class="form-control" value="${d.max_write_bit_block_size}" min="0" onkeydown="onlyNumbers(event)">
                    </div>
                    <div class="form-group">
                        <label for="wiz_max_write_register_block_size">Max Write Reg Block</label>
                        <input type="number" id="wiz_max_write_register_block_size" class="form-control" value="${d.max_write_register_block_size}" min="0" onkeydown="onlyNumbers(event)">
                    </div>
                <div class="wizard-actions">
                    <button type="button" class="btn btn-secondary" onclick="cancelWizard()">Cancel</button>
                    <button type="submit" class="btn btn-primary">Next →</button>
                </div>
            </form>
        </div>
    `;
}

function attachStep1Handlers() {
    document.getElementById('wizardStep1Form').onsubmit = (e) => {
        e.preventDefault();
        // Collect step 1 data
        const modelName = document.getElementById('wiz_model_name').value.trim();
        if (!modelName) {
            showToast('Model name is required', 'error');
            return;
        }
        if (modelName.length > 100) {
            showToast('Model name must be at most 100 characters', 'error');
            return;
        }

        // Validate numeric fields
        const timeoutRaw = document.getElementById('wiz_default_timeout_ms').value;
        if (timeoutRaw !== '') {
            const t = validateInt(timeoutRaw, 'Default timeout', {min: 0, max: 60000});
            if (!t.valid) { showToast(t.error, 'error'); return; }
        }

        const blockFields = [
            {id: 'wiz_max_read_bit_block_size', label: 'Max Read Bit Block'},
            {id: 'wiz_max_read_register_block_size', label: 'Max Read Register Block'},
            {id: 'wiz_max_write_bit_block_size', label: 'Max Write Bit Block'},
            {id: 'wiz_max_write_register_block_size', label: 'Max Write Register Block'},
        ];
        for (const f of blockFields) {
            const v = validateInt(document.getElementById(f.id).value, f.label, {min: 1, max: 256});
            if (!v.valid) { showToast(v.error, 'error'); return; }
        }

        const data = {
            model_name: modelName,
            protocol: document.getElementById('wiz_protocol').value,
            default_timeout_ms: timeoutRaw ? parseInt(timeoutRaw) : null,
            max_read_bit_block_size: parseInt(document.getElementById('wiz_max_read_bit_block_size').value) || 16,
            max_read_register_block_size: parseInt(document.getElementById('wiz_max_read_register_block_size').value) || 16,
            max_write_bit_block_size: parseInt(document.getElementById('wiz_max_write_bit_block_size').value) || 16,
            max_write_register_block_size: parseInt(document.getElementById('wiz_max_write_register_block_size').value) || 16
        };
        wizardState.data = data;

        wizardState.step = 2;
        renderWizardStep();
    };
}

// --- Step 2: I/O Point Definition ---

function renderWizardStep2() {
    const pts = wizardState.ioPoints;
    let rowsHtml = '';
    pts.forEach((p, idx) => {
        rowsHtml += renderIORow(p, idx);
    });

    return `
        <div class="card">
            <div class="card-title">📍 I/O Point Definitions</div>
            <p class="form-hint" style="margin-bottom: 12px;">Define all logical I/O points for this model. Each row represents one I/O point.</p>
            <div class="table-container" style="max-height: 55vh; overflow-y: auto;">
                <table id="ioPointsTable">
                    <thead>
                        <tr>
                            <th>Logical Addr *</th>
                            <th>Type *</th>
                            <th>Purpose *</th>
                            <th>Access *</th>
                            <th>Phys Addr</th>
                            <th>Method</th>
                            <th>Bitmask Off</th>
                            <th>Reg Count</th>
                            <th>Endian</th>
                            <th>Default Label</th>
                            <th></th>
                        </tr>
                    </thead>
                    <tbody id="ioPointsBody">
                        ${rowsHtml}
                    </tbody>
                </table>
            </div>
            <div style="margin-top: 12px;">
                <button type="button" class="btn btn-secondary" onclick="addIORow()">➕ Add I/O Point</button>
                <button type="button" class="btn btn-secondary" onclick="addIORow(true)">➕ Add 8 Points</button>
            </div>
            <div class="wizard-actions">
                <button type="button" class="btn btn-secondary" onclick="wizardBack()">← Back</button>
                <button type="button" class="btn btn-secondary" onclick="cancelWizard()">Cancel</button>
                <button type="button" class="btn btn-primary" onclick="wizardStep2Next()" id="step2NextBtn">Next →</button>
            </div>
        </div>
    `;
}

function renderIORow(p, idx) {
    return `
        <tr class="io-row-editor" data-idx="${idx}" data-io-id="${p.io_definition_id || ''}" data-child-slot-index="${p._childSlotIndex != null ? p._childSlotIndex : ''}" data-child-io-def-id="${p._childIoDefId != null ? p._childIoDefId : ''}" data-original-address="${p._originalAddress != null ? p._originalAddress : ''}">
            <td><input type="number" class="form-control form-control-sm" value="${p.logical_address != null ? p.logical_address : ''}" data-field="logical_address" min="0" required onkeydown="onlyNumbers(event)"></td>
            <td>
                <select class="form-control form-control-sm" data-field="io_type">
                    <option value="bit" ${p.io_type === 'bit' ? 'selected' : ''}>bit</option>
                    <option value="register" ${p.io_type === 'register' ? 'selected' : ''}>register</option>
                </select>
            </td>
            <td>
                <select class="form-control form-control-sm" data-field="purpose">
                    <option value="standard" ${p.purpose === 'standard' ? 'selected' : ''}>standard</option>
                    <option value="secure_state" ${p.purpose === 'secure_state' ? 'selected' : ''}>secure_state</option>
                    <option value="config" ${p.purpose === 'config' ? 'selected' : ''}>config</option>
                </select>
            </td>
            <td>
                <select class="form-control form-control-sm" data-field="hardware_access">
                    <option value="readonly" ${p.hardware_access === 'readonly' ? 'selected' : ''}>readonly</option>
                    <option value="readwrite" ${p.hardware_access === 'readwrite' ? 'selected' : ''}>readwrite</option>
                </select>
            </td>
            <td><input type="number" class="form-control form-control-sm" value="${p.physical_address != null ? p.physical_address : ''}" data-field="physical_address" min="0" onkeydown="onlyNumbers(event)"></td>
            <td>
                <select class="form-control form-control-sm" data-field="access_method">
                    <option value="direct" ${p.access_method === 'direct' ? 'selected' : ''}>direct</option>
                    <option value="bitmask" ${p.access_method === 'bitmask' ? 'selected' : ''}>bitmask</option>
                </select>
            </td>
            <td><input type="number" class="form-control form-control-sm" value="${p.bitmask_offset != null ? p.bitmask_offset : ''}" data-field="bitmask_offset" min="0" onkeydown="onlyNumbers(event)"></td>
            <td><input type="number" class="form-control form-control-sm" value="${p.register_count || 1}" data-field="register_count" min="1" onkeydown="onlyNumbers(event)"></td>
            <td>
                <select class="form-control form-control-sm" data-field="endianess">
                    <option value="big" ${p.endianess === 'big' ? 'selected' : ''}>big</option>
                    <option value="little" ${p.endianess === 'little' ? 'selected' : ''}>little</option>
                </select>
            </td>
            <td><input type="text" class="form-control form-control-sm" value="${p.default_io_label || ''}" data-field="default_io_label" placeholder="default label"></td>
            <td><button type="button" class="btn btn-danger btn-sm btn-icon" onclick="removeIORow(${idx})" title="Remove">✕</button></td>
        </tr>
    `;
}

function attachStep2Handlers() {
    // Nothing special needed — event handlers are inline
}

function addIORow(bulk) {
    collectIOPoints();
    const count = bulk ? 8 : 1;
    const lastAddr = wizardState.ioPoints.length > 0 ? Math.max(...wizardState.ioPoints.map(p => p.logical_address || 0)) : -1;
    for (let i = 0; i < count; i++) {
        wizardState.ioPoints.push({
            logical_address: lastAddr + 1 + i,
            io_type: 'bit',
            purpose: 'standard',
            hardware_access: 'readwrite',
            physical_address: lastAddr + 1 + i,
            access_method: 'direct',
            bitmask_offset: null,
            register_count: 1,
            endianess: 'big',
            default_io_label: ''
        });
    }
    renderWizardStep();
}

function removeIORow(idx) {
    collectIOPoints();
    
    // Robustly handle secure state mappings during deletion:
    // 1. Resolve current index-based mappings to actual object references
    const securePts = wizardState.ioPoints.filter(p => p.purpose === 'secure_state');
    const standardPts = wizardState.ioPoints.filter(p => p.purpose === 'standard');
    
    // Store mappings as object pairs { securePt: obj, standardPt: obj }
    const objectMappings = [];
    if (wizardState.secureStateMappings) {
        wizardState.secureStateMappings.forEach(m => {
            let sPt = securePts[m.securePointIndex];
            let stdPt = standardPts[m.standardPointIndex];
            
            // Fix: In edit mode, mappings use IDs, not indices. Resolve them.
            if (!sPt && m.fk_secure_state_io_definition_id) {
                sPt = securePts.find(p => p.io_definition_id === m.fk_secure_state_io_definition_id);
            }
            if (!stdPt && m.fk_standard_io_definition_id) {
                stdPt = standardPts.find(p => p.io_definition_id === m.fk_standard_io_definition_id);
            }

            if (sPt && stdPt) {
                objectMappings.push({ securePt: sPt, standardPt: stdPt });
            }
        });
    }
    
    // 2. Remove the point from the main array
    const removedPt = wizardState.ioPoints[idx];
    wizardState.ioPoints.splice(idx, 1);
    
    // 3. Re-build index-based mappings from the object pairs
    //    (Using the new filtered arrays after deletion)
    const newSecurePts = wizardState.ioPoints.filter(p => p.purpose === 'secure_state');
    const newStandardPts = wizardState.ioPoints.filter(p => p.purpose === 'standard');
    
    wizardState.secureStateMappings = [];
    objectMappings.forEach(pair => {
        // If either point was the one removed, drop the mapping
        if (pair.securePt === removedPt || pair.standardPt === removedPt) return;
        
        // Find new indices
        const sIdx = newSecurePts.indexOf(pair.securePt);
        const stdIdx = newStandardPts.indexOf(pair.standardPt);
        
        if (sIdx !== -1 && stdIdx !== -1) {
            wizardState.secureStateMappings.push({
                securePointIndex: sIdx,
                standardPointIndex: stdIdx
            });
        }
    });
    
    renderWizardStep();
}

function collectIOPoints() {
    const rows = document.querySelectorAll('.io-row-editor');
    const pts = [];
    rows.forEach(row => {
        const get = (field) => {
            const el = row.querySelector(`[data-field="${field}"]`);
            return el ? el.value : null;
        };
        const getNum = (field) => {
            const v = get(field);
            return v !== '' && v != null ? parseInt(v) : null;
        };
        // Preserve io_definition_id from the data attribute (for edit mode)
        const ioId = row.dataset.ioId;
        const pt = {
            logical_address: getNum('logical_address'),
            io_type: get('io_type'),
            purpose: get('purpose'),
            hardware_access: get('hardware_access'),
            physical_address: getNum('physical_address'),
            access_method: get('access_method'),
            bitmask_offset: getNum('bitmask_offset'),
            register_count: getNum('register_count') || 1,
            endianess: get('endianess') || 'big',
            default_io_label: get('default_io_label') || ''
        };
        if (ioId) pt.io_definition_id = parseInt(ioId);
        
        // Preserve aggregated child metadata (for aggregated_io_map and auto-configure)
        const childSlotIdx = row.dataset.childSlotIndex;
        const childIoDefId = row.dataset.childIoDefId;
        const origAddr = row.dataset.originalAddress;
        if (childSlotIdx !== '') pt._childSlotIndex = parseInt(childSlotIdx);
        if (childIoDefId !== '') pt._childIoDefId = parseInt(childIoDefId);
        if (origAddr !== '') pt._originalAddress = parseInt(origAddr);
        
        pts.push(pt);
    });
    wizardState.ioPoints = pts;
}

function wizardStep2Next() {
    collectIOPoints();

    // Validate: at least one IO point
    if (wizardState.ioPoints.length === 0) {
        showToast('Add at least one I/O point', 'error');
        return;
    }
    // Validate: all required fields present + ranges
    for (let i = 0; i < wizardState.ioPoints.length; i++) {
        const p = wizardState.ioPoints[i];
        if (p.logical_address == null) {
            showToast(`Row ${i + 1}: Logical address is required`, 'error');
            return;
        }
        if (p.logical_address < 0 || p.logical_address > 65535) {
            showToast(`Row ${i + 1}: Logical address must be 0–65535`, 'error');
            return;
        }
        if (p.physical_address != null && (p.physical_address < 0 || p.physical_address > 65535)) {
            showToast(`Row ${i + 1}: Physical address must be 0–65535`, 'error');
            return;
        }
        if (p.access_method === 'bitmask' && p.bitmask_offset != null && (p.bitmask_offset < 0 || p.bitmask_offset > 15)) {
            showToast(`Row ${i + 1}: Bitmask offset must be 0–15`, 'error');
            return;
        }
        if (p.register_count != null && (p.register_count < 1 || p.register_count > 4)) {
            showToast(`Row ${i + 1}: Register count must be 1–4`, 'error');
            return;
        }
    }

    // Check for duplicate logical addresses (same io_type + purpose combination)
    const seen = new Set();
    for (let i = 0; i < wizardState.ioPoints.length; i++) {
        const p = wizardState.ioPoints[i];
        const key = `${p.io_type}:${p.purpose}:${p.logical_address}`;
        if (seen.has(key)) {
            showToast(`Row ${i + 1}: Duplicate logical address ${p.logical_address} for ${p.io_type}/${p.purpose}`, 'error');
            return;
        }
        seen.add(key);
    }

    // Go to the next step (step 3 for non-agg, step 4 for agg since I/O is at step 3 for agg)
    wizardState.step++;
    renderWizardStep();
}

// --- Step 3: Secure State Mapping ---

function renderWizardStep3() {
    const securePts = wizardState.ioPoints.filter(p => p.purpose === 'secure_state');
    const standardPts = wizardState.ioPoints.filter(p => p.purpose === 'standard');

    let contentHtml = '';

    // Robustness: Clean up any invalid mappings before rendering
    // A mapping is invalid if it uses INDICES and those indices are out of bounds.
    // Mappings using IDs (from edit mode) are assumed valid until matched.
    wizardState.secureStateMappings = wizardState.secureStateMappings.filter(m => {
        // If it's an ID-based mapping (from DB), keep it
        if (m.fk_secure_state_io_definition_id) return true;
        // If it's an index-based mapping (from UI), check bounds
        return securePts[m.securePointIndex] && standardPts[m.standardPointIndex];
    });

    if (securePts.length === 0) {
        contentHtml = `
            <div class="empty-state" style="padding: 30px 0;">
                <div class="empty-state-icon">🔓</div>
                <p>No secure state I/O points defined.</p>
                <p class="form-hint">To create mappings, go back to Step 2 and set the purpose of one or more I/O points to <strong>secure_state</strong>.</p>
            </div>
        `;
    } else {
        let rowsHtml = securePts.map((sp, idx) => {
            // Find existing mapping for this secure point
            // 1. By io_definition_id (edit mode)
            let existingStdIdx = null;
            const existingById = wizardState.secureStateMappings.find(m => {
                if (wizardState.mode === 'edit' && sp.io_definition_id) {
                    return m.fk_secure_state_io_definition_id === sp.io_definition_id;
                }
                return false;
            });
            if (existingById) {
                existingStdIdx = standardPts.findIndex(s => s.io_definition_id === existingById.fk_standard_io_definition_id);
            }
            // 2. By securePointIndex (auto-configure / create mode)
            if (existingStdIdx == null || existingStdIdx < 0) {
                const existingByIdx = wizardState.secureStateMappings.find(m =>
                    m.securePointIndex === idx
                );
                if (existingByIdx != null) {
                    existingStdIdx = existingByIdx.standardPointIndex;
                }
            }

            return `
                <tr>
                    <td>
                        <span class="badge badge-warning">secure</span>
                        [${sp.io_type}@${sp.logical_address}] ${sp.default_io_label || '—'}
                    </td>
                    <td>↔</td>
                    <td>
                        <select class="form-control" data-secure-idx="${idx}">
                            <option value="">— Not mapped —</option>
                            ${standardPts.map((stdp, stdIdx) => {
                                const isSelected = existingStdIdx === stdIdx;
                                return `<option value="${stdIdx}" ${isSelected ? 'selected' : ''}>[${stdp.io_type}@${stdp.logical_address}] ${stdp.default_io_label || ''}</option>`;
                            }).join('')}
                        </select>
                    </td>
                </tr>
            `;
        }).join('');

        contentHtml = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>Secure State Point</th>
                            <th></th>
                            <th>Standard Reference</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${rowsHtml}
                    </tbody>
                </table>
            </div>
        `;
    }

    return `
        <div class="card">
            <div class="card-title">🔒 Secure State Mapping</div>
            <p class="form-hint" style="margin-bottom: 12px;">Map each secure state I/O point to the standard I/O point it protects. When a safe state is triggered, the value of the secure state point will be applied to its linked standard output.</p>
            ${contentHtml}
            <div class="wizard-actions">
                ${isAggregatedModel() && securePts.length > 0
                    ? '<button type="button" class="btn btn-accent" onclick="configureDefaultSecureState()" style="margin-right: auto;">⚡ Auto-Configure from Children</button>'
                    : ''
                }
                <button type="button" class="btn btn-secondary" onclick="wizardBack()">← Back</button>
                <button type="button" class="btn btn-secondary" onclick="cancelWizard()">Cancel</button>
                <button type="button" class="btn btn-success" onclick="collectStep3AndSave()">✅ Finish</button>
            </div>
        </div>
    `;
}

function attachStep3Handlers() {
    // Nothing special — event handlers are inline
}

function collectStep3AndSave() {
    const securePts = wizardState.ioPoints.filter(p => p.purpose === 'secure_state');
    const standardPts = wizardState.ioPoints.filter(p => p.purpose === 'standard');
    const selects = document.querySelectorAll('[data-secure-idx]');
    wizardState.secureStateMappings = [];

    selects.forEach(sel => {
        const secIdx = parseInt(sel.dataset.secureIdx);
        const stdIdx = sel.value;
        if (stdIdx !== '') {
            wizardState.secureStateMappings.push({
                securePointIndex: secIdx, // index into securePts
                standardPointIndex: parseInt(stdIdx) // index into standardPts
            });
        }
    });

    saveModelWizard();
}

// collectStep3AndNext removed — Secure State is now always the last step

// --- Step 4: Aggregated Child Models ---

async function renderWizardStep4() {
    // Ensure models list is loaded
    if (models.length === 0) {
        try {
            const res = await fetch('/api/models');
            models = await res.json();
        } catch (e) { /* ignore */ }
    }
    const children = wizardState.aggregatedChildren;
    // Load models for the select options (use the global models list)
    // Allow nested aggregation: Include ALL models except the current one (to prevent direct recursion)
    const availableModels = models.filter(m => {
        // If editing, exclude self. If creating, we don't have an ID yet (or it's null).
        return wizardState.mode !== 'edit' || m.model_id !== wizardState.editId;
    });
    
    // const modelOpts is not really used in the loop below, but if we updated the name we should be consistent
    // const modelOpts = availableModels.map(m => `<option value="${m.model_id}">${m.model_name}</option>`).join('');

    let rowsHtml = children.map((child, idx) => {
        return `
            <tr>
                <td class="text-center"><strong>${idx}</strong></td>
                <td>
                    <select class="form-control" data-child-slot="${idx}">
                        <option value="">-- Select Model --</option>
                        ${availableModels.map(m =>
                            `<option value="${m.model_id}" ${child.fk_child_model_id == m.model_id ? 'selected' : ''}>${m.model_name}</option>`
                        ).join('')}
                    </select>
                </td>
                <td class="table-actions">
                    <button class="btn btn-danger btn-sm btn-icon" onclick="removeAggChild(${idx})" title="Remove">🗑️</button>
                </td>
            </tr>
        `;
    }).join('');

    if (children.length === 0) {
        rowsHtml = '<tr><td colspan="3" class="text-center text-muted">No child models yet. Click "+ Add Child" to define slots.</td></tr>';
    }

    return `
        <div class="card">
            <div class="card-title">🧩 Aggregated Child Models</div>
            <p class="form-hint" style="margin-bottom: 12px;">Define the child models that compose this aggregated model. The slot index determines the order in which children are expected in the device's <code>connection_string</code>.</p>
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th style="width: 60px;">Slot</th>
                            <th>Child Model</th>
                            <th style="width: 80px;">Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${rowsHtml}
                    </tbody>
                </table>
            </div>
            <div style="margin-top: 10px;">
                <button type="button" class="btn btn-secondary btn-sm" onclick="addAggChild()">+ Add Child</button>
            </div>
            <div class="wizard-actions">
                <button type="button" class="btn btn-secondary" onclick="wizardBack()">← Back</button>
                <button type="button" class="btn btn-secondary" onclick="cancelWizard()">Cancel</button>
                <button type="button" class="btn btn-primary" onclick="collectChildModelsAndAutoPopulate()">Next →</button>
            </div>
        </div>
    `;
}

function attachStep4Handlers() {
    // Nothing special — event handlers are inline
}

function addAggChild() {
    collectAggChildren();
    wizardState.aggregatedChildren.push({ fk_child_model_id: null });
    renderWizardStep();
}

function removeAggChild(idx) {
    collectAggChildren();
    wizardState.aggregatedChildren.splice(idx, 1);
    renderWizardStep();
}

function collectAggChildren() {
    const selects = document.querySelectorAll('[data-child-slot]');
    selects.forEach(sel => {
        const idx = parseInt(sel.dataset.childSlot);
        const val = sel.value;
        if (wizardState.aggregatedChildren[idx]) {
            wizardState.aggregatedChildren[idx].fk_child_model_id = val ? parseInt(val) : null;
        }
    });
}

async function collectChildModelsAndAutoPopulate() {
    collectAggChildren();
    // Validate: all slots must have a model selected
    for (let i = 0; i < wizardState.aggregatedChildren.length; i++) {
        if (!wizardState.aggregatedChildren[i].fk_child_model_id) {
            showToast(`Slot ${i}: Please select a child model`, 'error');
            return;
        }
    }
    if (wizardState.aggregatedChildren.length === 0) {
        showToast('Aggregated models must have at least one child model', 'error');
        return;
    }
    // Auto-populate I/O points from children
    // If in edit mode and config hasn't changed, preserve existing IO points (don't resurrect deleted ones)
    let shouldRepopulate = true;
    if (wizardState.mode === 'edit' && wizardState.initialAggregatedChildrenJson) {
         if (JSON.stringify(wizardState.aggregatedChildren) === wizardState.initialAggregatedChildrenJson && wizardState.ioPoints.length > 0) {
             shouldRepopulate = false;
         }
    }

    if (shouldRepopulate) {
        await autoPopulateAggregatedIO();
    }

    wizardState.step++;
    renderWizardStep();
}

/**
 * Fetches each child model's full IO definitions and secure state mappings,
 * then creates aggregated IO points with offset addresses.
 * Each IO point carries _childSlotIndex and _childIoDefId metadata for aggregated_io_map.
 */
async function autoPopulateAggregatedIO() {
    let nextAddress = 0;
    wizardState.ioPoints = [];
    wizardState._childSecureStateMappings = [];  // For "Configure Default" button

    for (let slotIdx = 0; slotIdx < wizardState.aggregatedChildren.length; slotIdx++) {
        const childModelId = wizardState.aggregatedChildren[slotIdx].fk_child_model_id;
        try {
            const resp = await fetch(`/api/models/${childModelId}/full`);
            const childModel = await resp.json();
            const childIOs = childModel.io_definitions || [];
            const childSSM = childModel.secure_state_mappings || [];

            // Sort by logical_address to determine range
            const sorted = [...childIOs].sort((a, b) => a.logical_address - b.logical_address);
            const maxAddr = sorted.length > 0 ? sorted[sorted.length - 1].logical_address : -1;
            const offset = nextAddress;

            // Create aggregated IO points from child's IO definitions
            childIOs.forEach(io => {
                wizardState.ioPoints.push({
                    logical_address: io.logical_address + offset,
                    io_type: io.io_type,
                    purpose: io.purpose,
                    hardware_access: io.hardware_access,
                    physical_address: io.physical_address,
                    access_method: io.access_method || 'direct',
                    bitmask_offset: io.bitmask_offset,
                    register_count: io.register_count || 1,
                    endianess: io.endianess || 'big',
                    default_io_label: io.default_io_label || '',
                    // Metadata for aggregated_io_map (prefixed with _ so they don't get sent to API)
                    _childSlotIndex: slotIdx,
                    _childIoDefId: io.io_definition_id,
                    _originalAddress: io.logical_address
                });
            });

            // Store child's secure state mappings for "Configure Default" button
            childSSM.forEach(m => {
                wizardState._childSecureStateMappings.push({
                    childSlotIndex: slotIdx,
                    secureIoDefId: m.fk_secure_state_io_definition_id,
                    standardIoDefId: m.fk_standard_io_definition_id
                });
            });

            nextAddress = maxAddr + offset + 1;
        } catch (error) {
            console.error(`Failed to fetch child model ${childModelId}:`, error);
            showToast(`Failed to load IO definitions for child model in slot ${slotIdx}`, 'error');
        }
    }
    // Sort all IO points by logical_address for clean display
    wizardState.ioPoints.sort((a, b) => a.logical_address - b.logical_address);
}

/**
 * Auto-configures secure state mappings based on children's existing mappings.
 * Only works if the user hasn't modified the auto-populated IO addresses.
 */
async function configureDefaultSecureState() {
    // If _childSecureStateMappings is empty, try to fetch from API
    // (this happens in edit mode, or if the wizard was navigated back/forth)
    if (!wizardState._childSecureStateMappings || wizardState._childSecureStateMappings.length === 0) {
        wizardState._childSecureStateMappings = [];
        try {
            for (let slotIdx = 0; slotIdx < wizardState.aggregatedChildren.length; slotIdx++) {
                const childModelId = wizardState.aggregatedChildren[slotIdx].fk_child_model_id;
                if (!childModelId) continue;
                const resp = await fetch(`/api/models/${childModelId}/full`);
                const childModel = await resp.json();
                (childModel.secure_state_mappings || []).forEach(m => {
                    wizardState._childSecureStateMappings.push({
                        childSlotIndex: slotIdx,
                        secureIoDefId: m.fk_secure_state_io_definition_id,
                        standardIoDefId: m.fk_standard_io_definition_id
                    });
                });
            }
        } catch (e) {
            console.error('Failed to fetch child secure state mappings:', e);
        }
    }

    if (wizardState._childSecureStateMappings.length === 0) {
        showToast('No secure state mappings found in child models', 'info');
        return;
    }

    wizardState.secureStateMappings = [];
    const securePts = wizardState.ioPoints.filter(p => p.purpose === 'secure_state');
    const standardPts = wizardState.ioPoints.filter(p => p.purpose === 'standard');

    wizardState._childSecureStateMappings.forEach(csm => {
        // Find the aggregated IO point that came from this child's secure IO def
        const secureAggIdx = wizardState.ioPoints.findIndex(p =>
            p._childSlotIndex === csm.childSlotIndex && p._childIoDefId === csm.secureIoDefId
        );
        // Find the aggregated IO point that came from this child's standard IO def
        const standardAggIdx = wizardState.ioPoints.findIndex(p =>
            p._childSlotIndex === csm.childSlotIndex && p._childIoDefId === csm.standardIoDefId
        );

        if (secureAggIdx >= 0 && standardAggIdx >= 0) {
            // Find indices within their filtered arrays (securePts, standardPts)
            const secPtIdx = securePts.indexOf(wizardState.ioPoints[secureAggIdx]);
            const stdPtIdx = standardPts.indexOf(wizardState.ioPoints[standardAggIdx]);
            if (secPtIdx >= 0 && stdPtIdx >= 0) {
                wizardState.secureStateMappings.push({
                    securePointIndex: secPtIdx,
                    standardPointIndex: stdPtIdx
                });
            }
        }
    });

    showToast(`Auto-configured ${wizardState.secureStateMappings.length} secure state mapping(s) from children`, 'success');
    renderWizardStep();
}

// --- Wizard navigation helpers ---

function wizardBack() {
    const isAgg = isAggregatedModel();
    if (isAgg) {
        // Aggregated order: step 2 = Child Models, step 3 = I/O, step 4 = Secure State
        if (wizardState.step === 2) collectAggChildren();
        if (wizardState.step === 3) collectIOPoints();
    } else {
        if (wizardState.step === 2) collectIOPoints();
    }
    wizardState.step--;
    if (wizardState.step < 1) wizardState.step = 1;
    renderWizardStep();
}

function cancelWizard() {
    if (!confirm('Are you sure you want to cancel? All unsaved changes will be lost.')) return;
    loadPage('models');
}

// --- Save (Create or Edit) ---

async function saveModelWizard() {
    const d = wizardState.data;

    try {
        let modelId;

        // =====================================================================
        // STEP 1: Create or Update the model record itself
        // =====================================================================
        if (wizardState.mode === 'create') {
            const modelResp = await fetch('/api/models', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model_name: d.model_name,
                    interface_protocol: d.interface_protocol,
                    default_timeout_ms: d.default_timeout_ms,
                    max_read_bit_block_size: d.max_read_bit_block_size,
                    max_read_register_block_size: d.max_read_register_block_size,
                    max_write_bit_block_size: d.max_write_bit_block_size,
                    max_write_register_block_size: d.max_write_register_block_size
                })
            });
            const modelResult = await modelResp.json();
            if (!modelResp.ok) {
                showToast(modelResult.error || 'Failed to create model', 'error');
                return;
            }
            modelId = modelResult.id;
        } else {
            modelId = wizardState.editId;
            const modelResp = await fetch(`/api/models/${modelId}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model_name: d.model_name,
                    protocol: d.protocol,
                    default_timeout_ms: d.default_timeout_ms,
                    max_read_bit_block_size: d.max_read_bit_block_size,
                    max_read_register_block_size: d.max_read_register_block_size,
                    max_write_bit_block_size: d.max_write_bit_block_size,
                    max_write_register_block_size: d.max_write_register_block_size
                })
            });
            if (!modelResp.ok) {
                const r = await modelResp.json();
                showToast(r.error || 'Failed to update model', 'error');
                return;
            }
        }

        // =====================================================================
        // STEP 2: Smart Diff for IO Definitions (edit) or Batch Create (create)
        // =====================================================================
        // After this step, every IO point in wizardState.ioPoints will have a
        // definitive io_definition_id (either existing or newly assigned).
        // We build a parallel array `finalIds` aligned with wizardState.ioPoints.
        let finalIds = [];

        if (wizardState.mode === 'create') {
            // --- CREATE MODE: batch insert everything ---
            if (wizardState.ioPoints.length > 0) {
                const ioItems = wizardState.ioPoints.map(p => ({
                    fk_model_id: modelId,
                    logical_address: p.logical_address,
                    io_type: p.io_type,
                    purpose: p.purpose,
                    hardware_access: p.hardware_access,
                    physical_address: p.physical_address,
                    access_method: p.access_method,
                    bitmask_offset: p.bitmask_offset,
                    register_count: p.register_count,
                    endianess: p.endianess,
                    default_io_label: p.default_io_label
                }));
                const ioResp = await fetch('/api/io-definitions/batch', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ items: ioItems })
                });
                const ioResult = await ioResp.json();
                if (!ioResp.ok) {
                    showToast(ioResult.error || 'Model saved but failed to create IO definitions', 'error');
                    loadPage('models');
                    return;
                }
                finalIds = ioResult.ids;
            }
        } else {
            // --- EDIT MODE: Smart Diff (surgical insert/update/delete) ---
            const originalIds = wizardState._originalIoIds || [];
            const currentIds = wizardState.ioPoints
                .map(p => p.io_definition_id)
                .filter(id => id != null);

            // C (DELETE): IDs that were in originalIds but are no longer in the screen
            const idsToDelete = originalIds.filter(id => !currentIds.includes(id));

            // B (UPDATE): Points that have an existing io_definition_id
            const pointsToUpdate = wizardState.ioPoints.filter(p => p.io_definition_id != null);

            // A (INSERT): Points that don't have an io_definition_id (newly added)
            const pointsToInsert = wizardState.ioPoints.filter(p => p.io_definition_id == null);

            // --- Execute C: Delete removed IO points (BATCH) ---
            if (idsToDelete.length > 0) {
                const delResp = await fetch('/api/io-definitions/batch-delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ids: idsToDelete })
                });
                if (!delResp.ok) {
                    const r = await delResp.json();
                    showToast(r.error || 'Failed to delete unused I/O points', 'error');
                    return;
                }
            }

            // --- Execute B: Update existing IO points (BATCH) ---
            if (pointsToUpdate.length > 0) {
                const updateItems = pointsToUpdate.map(p => ({
                    io_definition_id: p.io_definition_id,
                    logical_address: p.logical_address,
                    io_type: p.io_type,
                    purpose: p.purpose,
                    hardware_access: p.hardware_access,
                    physical_address: p.physical_address,
                    access_method: p.access_method,
                    bitmask_offset: p.bitmask_offset,
                    register_count: p.register_count,
                    endianess: p.endianess,
                    default_io_label: p.default_io_label
                }));

                const putResp = await fetch('/api/io-definitions/batch-update', {
                    method: 'PUT',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ items: updateItems })
                });
                if (!putResp.ok) {
                    const r = await putResp.json();
                    showToast(r.error || 'Failed to update I/O definitions', 'error');
                    return;
                }
            }

            // --- Execute A: Batch insert new IO points ---
            let newIds = [];
            if (pointsToInsert.length > 0) {
                const ioItems = pointsToInsert.map(p => ({
                    fk_model_id: modelId,
                    logical_address: p.logical_address,
                    io_type: p.io_type,
                    purpose: p.purpose,
                    hardware_access: p.hardware_access,
                    physical_address: p.physical_address,
                    access_method: p.access_method,
                    bitmask_offset: p.bitmask_offset,
                    register_count: p.register_count,
                    endianess: p.endianess,
                    default_io_label: p.default_io_label
                }));
                const ioResp = await fetch('/api/io-definitions/batch', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ items: ioItems })
                });
                const ioResult = await ioResp.json();
                if (!ioResp.ok) {
                    showToast(ioResult.error || 'Model saved but failed to create new IO definitions', 'error');
                    loadPage('models');
                    return;
                }
                newIds = ioResult.ids;
            }

            // --- Build finalIds: aligned with wizardState.ioPoints ---
            // Assign the new DB IDs back to the newly inserted points
            let newIdCursor = 0;
            for (const p of pointsToInsert) {
                p.io_definition_id = newIds[newIdCursor++];
            }
            // Now every point has a definitive io_definition_id
            finalIds = wizardState.ioPoints.map(p => p.io_definition_id);
        }

        // =====================================================================
        // STEP 3: Rebuild secondary relationships using definitive IDs
        // =====================================================================

        // --- 3a: Secure State Mappings (safe to delete & re-create) ---
        if (wizardState.secureStateMappings.length > 0 && finalIds.length > 0) {
            const securePts = wizardState.ioPoints.filter(p => p.purpose === 'secure_state');
            const standardPts = wizardState.ioPoints.filter(p => p.purpose === 'standard');

            // Build index maps: filtered array index -> position in wizardState.ioPoints
            const secureIdxInAll = [];
            const standardIdxInAll = [];
            wizardState.ioPoints.forEach((p, i) => {
                if (p.purpose === 'secure_state') secureIdxInAll.push(i);
                if (p.purpose === 'standard') standardIdxInAll.push(i);
            });

            const ssmItems = wizardState.secureStateMappings.map(m => {
                let sIdx = m.securePointIndex;
                let stdIdx = m.standardPointIndex;

                // Resolve DB IDs to current indices (for edit mode, ID-based mappings)
                if (sIdx == null && m.fk_secure_state_io_definition_id) {
                    const sPt = securePts.find(p => p.io_definition_id === m.fk_secure_state_io_definition_id);
                    if (sPt) sIdx = securePts.indexOf(sPt);
                }
                if (stdIdx == null && m.fk_standard_io_definition_id) {
                    const stdPt = standardPts.find(p => p.io_definition_id === m.fk_standard_io_definition_id);
                    if (stdPt) stdIdx = standardPts.indexOf(stdPt);
                }

                if (sIdx != null && stdIdx != null &&
                    secureIdxInAll[sIdx] != null && standardIdxInAll[stdIdx] != null) {
                    return {
                        fk_model_id: modelId,
                        fk_secure_state_io_definition_id: finalIds[secureIdxInAll[sIdx]],
                        fk_standard_io_definition_id: finalIds[standardIdxInAll[stdIdx]]
                    };
                }
                return null;
            }).filter(item => item !== null);

            if (ssmItems.length > 0) {
                const ssmResp = await fetch('/api/secure-state-mapping/batch', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ model_id: modelId, items: ssmItems })
                });
                if (!ssmResp.ok) {
                    const ssmResult = await ssmResp.json();
                    showToast(ssmResult.error || 'Model saved but failed to save secure state mappings', 'error');
                    loadPage('models');
                    return;
                }
            }
        }

        // --- 3b: Aggregated model children (if applicable) ---
        if (isAggregatedModel() && wizardState.aggregatedChildren.length > 0) {
            const childResp = await fetch('/api/aggregated-model-children/batch', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model_id: modelId,
                    children: wizardState.aggregatedChildren
                })
            });
            if (!childResp.ok) {
                const childResult = await childResp.json();
                showToast(childResult.error || 'Model saved but failed to create child model slots', 'error');
                loadPage('models');
                return;
            }
        }

        // --- 3c: Aggregated IO map (if applicable) ---
        if (isAggregatedModel() && wizardState.ioPoints.some(p => p._childIoDefId)) {
            const ioMapItems = [];
            wizardState.ioPoints.forEach((p, idx) => {
                if (p._childSlotIndex != null && p._childIoDefId != null && finalIds[idx]) {
                    ioMapItems.push({
                        fk_aggregated_io_definition_id: finalIds[idx],
                        child_slot_index: p._childSlotIndex,
                        fk_child_io_definition_id: p._childIoDefId
                    });
                }
            });
            if (ioMapItems.length > 0) {
                const mapResp = await fetch('/api/aggregated-io-map/batch', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ model_id: modelId, items: ioMapItems })
                });
                if (!mapResp.ok) {
                    const mapResult = await mapResp.json();
                    showToast(mapResult.error || 'Model saved but failed to create IO map entries', 'error');
                    loadPage('models');
                    return;
                }
            }
        }

        showToast(`Model ${wizardState.mode === 'create' ? 'created' : 'updated'} successfully!`, 'success');
        loadPage('models');
    } catch (error) {
        showToast('Failed to save model: ' + error.message, 'error');
    }
}

// Keep old function names for backward compatibility with other pages
async function loadModels() { await loadModelsPage(); }
function showCreateModelForm() { showModelWizard(); }
async function editModel(id) { await editModelWizard(id); }
function showCreateModelForm() { showModelWizard(); }

// =============================================================================
// DEVICES
// =============================================================================


// =============================================================================
// DEVICES — 2-Step Wizard
// =============================================================================

let deviceWizardState = {
    mode: 'create',
    deviceId: null,
    step: 1,
    step1Data: {},   // Device basic info
    ioConfig: []     // IO configuration for Step 2
};

async function showDeviceWizard() {
    if (models.length === 0) {
        try {
            const res = await fetch('/api/models');
            models = await res.json();
        } catch (e) {
            showToast('Failed to load models list', 'error');
        }
    }

    deviceWizardState = {
        mode: 'create',
        deviceId: null,
        step: 1,
        step1Data: {
            module_name: '',
            channel: 'spi',
            connection_string: '',
            address_on_channel: '',
            fk_model_id: null,
            timeout_ms: 1000
        },
        ioConfig: []
    };
    
    showModal('New Device', '<div id="deviceWizardContainer"></div>');
    renderDeviceWizard();
}

async function editDevice(id) {
    if (models.length === 0) {
        try {
            const res = await fetch('/api/models');
            models = await res.json();
        } catch (e) {
            showToast('Failed to load models list', 'error');
        }
    }

    try {
        const response = await fetch(`/api/devices/${id}`);
        const device = await response.json();
        
        deviceWizardState = {
            mode: 'edit',
            deviceId: id,
            step: 1,
            step1Data: {
                module_name: device.module_name,
                channel: device.channel,
                connection_string: device.connection_string,
                address_on_channel: device.address_on_channel,
                fk_model_id: device.fk_model_id,
                timeout_ms: device.timeout_ms
            },
            originalModelId: device.fk_model_id,
            ioConfig: []
        };
        
        showModal('Edit Device', '<div id="deviceWizardContainer"></div>');
        renderDeviceWizard();
    } catch (error) {
        showToast('Failed to load device details', 'error');
    }
}

function renderDeviceWizard() {
    const container = document.getElementById('deviceWizardContainer');
    if (!container) return;

    const step = deviceWizardState.step;
    const steps = ['Device Config', 'I/O Configuration'];
    
    let stepperHtml = '<div class="wizard-stepper">';
    for (let i = 1; i <= steps.length; i++) {
        const cls = i === step ? 'active' : (i < step ? 'completed' : '');
        stepperHtml += `
            <div class="wizard-step ${cls}">
                <div class="wizard-step-circle">${i < step ? '✓' : i}</div>
                <div class="wizard-step-label">${steps[i - 1]}</div>
            </div>
            ${i < steps.length ? '<div class="wizard-step-line ' + (i < step ? 'completed' : '') + '"></div>' : ''}
        `;
    }
    stepperHtml += '</div>';

    let bodyHtml = '';
    if (step === 1) {
        setModalWide(false);
        bodyHtml = renderDeviceWizardStep1();
    } else if (step === 2) {
        setModalWide(true);
        bodyHtml = renderDeviceWizardStep2();
    }

    container.innerHTML = stepperHtml + '<div class="wizard-body">' + bodyHtml + '</div>';

    if (step === 1) attachDeviceStep1Handlers();
    if (step === 2) attachDeviceStep2Handlers();
}

function renderDeviceWizardStep1() {
    const d = deviceWizardState.step1Data;
    
    // Sort models by name
    const sortedModels = [...models].sort((a, b) => a.model_name.localeCompare(b.model_name));
    
    const modelOptions = sortedModels.map(m => 
        `<option value="${m.model_id}" data-protocol="${m.protocol}" ${d.fk_model_id == m.model_id ? 'selected' : ''}>${m.model_name}</option>`
    ).join('');

    return `
        <form id="deviceStep1Form">
            <!-- 1. Name & Model -->
            <div class="form-row">
                <div class="form-group" style="flex: 1;">
                    <label for="wiz_module_name">Device Name *</label>
                    <input type="text" id="wiz_module_name" class="form-control" required placeholder="e.g., Slot 0 - 8SD" value="${d.module_name || ''}">
                </div>
                <div class="form-group" style="flex: 1;">
                    <label for="wiz_fk_model_id">Model *</label>
                    <select id="wiz_fk_model_id" class="form-control" required onchange="updateDeviceStep1UI()">
                        <option value="">-- Select Model --</option>
                        ${modelOptions}
                    </select>
                </div>
            </div>

            <!-- 2. Protocol & Timeout -->
            <div class="form-row">
                <!-- Protocol Display (Readonly) -->
                <div class="form-group" style="flex: 1;">
                    <label>Protocol</label>
                    <input type="text" id="wiz_protocol_display" class="form-control" readonly value="${d.protocol || ''}">
                </div>

                <!-- Timeout: Only for Modbus -->
                <div class="form-group" style="flex: 1; display: none;" id="group_timeout">
                    <label for="wiz_timeout_ms">Timeout (ms)</label>
                    <input type="number" id="wiz_timeout_ms" class="form-control" value="${d.timeout_ms || 1000}" onkeydown="onlyNumbers(event)">
                </div>
            </div>

            <!-- 3. Channel (Hidden) -->
            <div style="display: none;">
                <select id="wiz_channel">
                    <option value="spi" ${d.channel === 'spi' ? 'selected' : ''}>SPI</option>
                    <option value="rs485" ${d.channel === 'rs485' ? 'selected' : ''}>RS-485 Internal</option>
                    <option value="tcp" ${d.channel === 'tcp' ? 'selected' : ''}>TCP/IP</option>
                    <option value="aggregated" ${d.channel === 'aggregated' ? 'selected' : ''}>Aggregated</option>
                </select>
            </div>

            <!-- 4. Connection Details -->
            <div class="form-row">
                <!-- Child Devices (Aggregated Only) -->
                <div class="form-group" style="flex: 2; display: none;" id="group_child_devices">
                    <label>🧩 Child Devices (by slot)</label>
                    <div id="childDeviceSlots">Loading...</div>
                </div>

                <!-- Connection String (Serial Port / IP) -->
                <div class="form-group" style="flex: 2;" id="group_connection_string">
                    <label id="label_connection_string">Connection String *</label>
                    <input type="text" id="wiz_connection_string" class="form-control" value="${d.connection_string || ''}">
                </div>

                <!-- Address -->
                <div class="form-group" style="flex: 1;" id="group_address">
                    <label for="wiz_address_on_channel">Address *</label>
                    <input type="text" id="wiz_address_on_channel" class="form-control" value="${d.address_on_channel || ''}" onkeydown="onlyNumbers(event)">
                </div>
            </div>

            <div class="wizard-actions">
                <button type="button" class="btn btn-secondary" onclick="closeModal()">Cancel</button>
                <button type="button" class="btn btn-primary" onclick="collectDeviceStep1AndNext()">Next →</button>
            </div>
        </form>
    `;
}

function updateDeviceStep1UI() {
    const modelSelect = document.getElementById('wiz_fk_model_id');
    const channelSelect = document.getElementById('wiz_channel');
    const protocolDisplay = document.getElementById('wiz_protocol_display');
    
    // Auto-update protocol
    const selectedOption = modelSelect.options[modelSelect.selectedIndex];
    const protocol = selectedOption ? selectedOption.getAttribute('data-protocol') : '';
    protocolDisplay.value = protocol || '';

    // Translation logic
    let channel = 'tcp';

    switch (protocol) {
        case 'borrell-spi':
            channel = 'spi';
            break;
        case 'modbus-tcp':
            channel = 'tcp';
            break;
        case 'modbus-rtu':
            channel = channelSelect.value.includes('rs485') ? channelSelect.value : 'rs485';
            break;
        case 'aggregated':
            channel = 'aggregated';
            break;
    }

    // Update UI
    if (protocol && protocol !== 'modbus-rtu') {
        channelSelect.value = channel;
        channelSelect.disabled = true;
    } else {
        channelSelect.disabled = false;
    }
    
    // Visibility: Timeout (Modbus only)
    const isModbus = protocol && protocol.includes('modbus');
    document.getElementById('group_timeout').style.display = isModbus ? 'block' : 'none';
    
    const currentChannel = channelSelect.value;
    const groupConnStr = document.getElementById('group_connection_string');
    const labelConnStr = document.getElementById('label_connection_string');
    const inputConnStr = document.getElementById('wiz_connection_string');
    const groupAddress = document.getElementById('group_address');
    const groupChildDevices = document.getElementById('group_child_devices');
    
    // Reset defaults
    groupConnStr.style.display = 'block';
    groupAddress.style.display = 'block';
    groupChildDevices.style.display = 'none';
    labelConnStr.textContent = 'Connection String *';
    inputConnStr.placeholder = '';
    
    // Logic based on Protocol AND Channel
    if (currentChannel === 'aggregated') {
        groupAddress.style.display = 'none'; // No address for aggregated
        groupConnStr.style.display = 'none'; // No connection string (auto-built)
        groupChildDevices.style.display = 'block';
        
        // Load child model slots and render selectors
        const selectedModelId = modelSelect.value;
        if (selectedModelId) {
            loadChildDeviceSelectors(parseInt(selectedModelId));
        } else {
            document.getElementById('childDeviceSlots').innerHTML = '<span class="text-muted">Select a model first</span>';
        }
    } else if (currentChannel === 'spi') {
        groupConnStr.style.display = 'none'; // SPI: Connection string hidden
    } else {
        // RS485 or TCP
        if (protocol === 'modbus-tcp' || currentChannel === 'tcp') {
            labelConnStr.textContent = 'IP Address *';
            inputConnStr.placeholder = '192.168.1.50:502';
        } else if (protocol === 'modbus-rtu' || currentChannel.includes('rs485')) {
            labelConnStr.textContent = 'Serial Port *';
            inputConnStr.placeholder = '/dev/ttyAS5';
        }
    }
}

async function loadChildDeviceSelectors(aggregatedModelId) {
    const slotsContainer = document.getElementById('childDeviceSlots');
    if (!slotsContainer) return;
    
    try {
        // Fetch child model definitions for this aggregated model
        const childrenResp = await fetch(`/api/aggregated-model-children?model_id=${aggregatedModelId}`);
        const childSlots = await childrenResp.json();
        
        if (childSlots.length === 0) {
            slotsContainer.innerHTML = '<span class="text-muted">No child model slots defined for this aggregated model. Edit the model to add children.</span>';
            return;
        }
        
        // Fetch all devices to populate the selectors
        const devicesResp = await fetch('/api/devices');
        const allDevices = await devicesResp.json();
        
        // Parse existing connection_string to pre-select devices
        const existingConnStr = deviceWizardState.step1Data.connection_string || '';
        let existingChildIds = [];
        if (existingConnStr.startsWith('aggregator:')) {
            existingChildIds = existingConnStr.replace('aggregator:', '').split(';').map(s => parseInt(s.trim())).filter(n => !isNaN(n));
        }
        
        let html = '';
        childSlots.forEach((slot, idx) => {
            // Filter devices to only show those with the expected model
            const matchingDevices = allDevices.filter(d => d.fk_model_id === slot.fk_child_model_id);
            const selectedId = existingChildIds[idx] || null;
            
            html += `
                <div class="form-group" style="margin-bottom: 8px;">
                    <label style="font-size: 0.85rem;">Slot ${idx}: <strong>${slot.child_model_name}</strong></label>
                    <select class="form-control" data-child-device-slot="${idx}">
                        <option value="">-- Select Device --</option>
                        ${matchingDevices.map(d => 
                            `<option value="${d.module_id}" ${selectedId === d.module_id ? 'selected' : ''}>${d.module_name} (ID: ${d.module_id})</option>`
                        ).join('')}
                    </select>
                    ${matchingDevices.length === 0 ? '<span class="form-hint" style="color: var(--warning);">⚠️ No devices of this model type exist yet. Create one first.</span>' : ''}
                </div>
            `;
        });
        
        slotsContainer.innerHTML = html;
    } catch (error) {
        slotsContainer.innerHTML = '<span class="text-muted">Failed to load child model slots</span>';
        console.error('Failed to load child device selectors:', error);
    }
}

function attachDeviceStep1Handlers() {
    // Initial UI update
    updateDeviceStep1UI();
}

async function collectDeviceStep1AndNext() {
    // Collect data
    const moduleName = document.getElementById('wiz_module_name').value.trim();
        if (!moduleName) {
            showToast('Module name is required', 'error');
            return;
        }
        if (moduleName.length > 100) {
            showToast('Module name must be at most 100 characters', 'error');
            return;
        }

        const timeoutVal = validateInt(document.getElementById('wiz_timeout_ms').value, 'Timeout', {min: 0, max: 60000});
        if (!timeoutVal.valid) { showToast(timeoutVal.error, 'error'); return; }

        const fkModelId = document.getElementById('wiz_fk_model_id').value;
        if (!fkModelId) {
            showToast('Please select a model', 'error');
            return;
        }

        const data = {
            module_name: moduleName,
            channel: document.getElementById('wiz_channel').value,
            protocol: document.getElementById('wiz_protocol_display').value,
            connection_string: document.getElementById('wiz_connection_string').value,
            address_on_channel: document.getElementById('wiz_address_on_channel').value,
            fk_model_id: parseInt(fkModelId),
            timeout_ms: timeoutVal.value != null ? timeoutVal.value : 1000
        };
        
        // For aggregated models, build connection_string from child device selectors
        if (data.channel === 'aggregated') {
            const childSelects = document.querySelectorAll('[data-child-device-slot]');
            const childIds = [];
            let allSelected = true;
            childSelects.forEach(sel => {
                const val = sel.value;
                if (!val) {
                    allSelected = false;
                } else {
                    childIds.push(val);
                }
            });
            if (childSelects.length > 0 && !allSelected) {
                showToast('Please select a device for each child slot', 'error');
                return;
            }
            if (childIds.length > 0) {
                data.connection_string = 'aggregator:' + childIds.join(';');
            }
            data.address_on_channel = '0'; // Not used for aggregated
        }
        
        // Validate
        if (data.channel !== 'spi' && data.channel !== 'aggregated' && !data.connection_string) {
             showToast('Connection string/IP/Port is required for this channel', 'error');
             return;
        }
        if (data.channel !== 'aggregated' && !data.address_on_channel) {
             showToast('Address is required', 'error');
             return;
        }
        
        // Just store the data locally — do NOT save to DB yet.
        // The actual create/update happens in finishDeviceWizard().
        deviceWizardState.step1Data = data;

        // Determine if model was changed during edit
        deviceWizardState.modelChanged = (deviceWizardState.mode === 'edit' && data.fk_model_id !== deviceWizardState.originalModelId);

        if (deviceWizardState.mode === 'edit' && deviceWizardState.deviceId && !deviceWizardState.modelChanged) {
            // Load current config from DB if model hasn't changed
            await loadDeviceWizardIOConfig();
        } else if (data.fk_model_id) {
            // Load default I/O configs from the model if creating, OR if the model changed during edit
            if (!deviceWizardState.ioConfig || deviceWizardState.ioConfig.length === 0 || deviceWizardState._lastModelId !== data.fk_model_id) {
                try {
                    const response = await fetch(`/api/models/${data.fk_model_id}/full`);
                    const modelData = await response.json();
                    
                    const ioDefs = modelData.io_definitions || [];
                    // Ensure the mock is ordered by logical_address so it matches the DB's module-io-config endpoint ordering
                    ioDefs.sort((a, b) => a.logical_address - b.logical_address);
                    
                    deviceWizardState.ioConfig = ioDefs.map(def => ({
                        fk_io_definition_id: def.io_definition_id,
                        logical_address: def.logical_address,
                        io_type: def.io_type,
                        purpose: def.purpose,
                        default_io_label: def.default_io_label || '',
                        user_label: def.default_io_label || '',
                        units: '',
                        scale_factor: 1.0,
                        offset: 0.0,
                        visibility: 'visible',
                        visibility_mode: 'periodically',
                        refresh_rate: 1,
                        sync: true
                    }));
                    deviceWizardState._lastModelId = data.fk_model_id;
                } catch (error) {
                    console.error('Failed to load default I/O configuration for model', error);
                    deviceWizardState.ioConfig = [];
                }
            }
        }

    deviceWizardState.step = 2;
    renderDeviceWizard();
}

async function loadDeviceWizardIOConfig() {
    try {
        const response = await fetch(`/api/module-io-config?module_id=${deviceWizardState.deviceId}`);
        const configs = await response.json();
        deviceWizardState.ioConfig = configs;
    } catch (error) {
        showToast('Failed to load I/O configuration', 'error');
        deviceWizardState.ioConfig = [];
    }
}

function renderDeviceWizardStep2() {
    const configs = deviceWizardState.ioConfig;
    
    if (configs.length === 0) {
        return `
            <div class="empty-state">
                <p>No I/O points available for this device model.</p>
                <div class="wizard-actions">
                    <button type="button" class="btn btn-secondary" onclick="deviceWizardBack()">← Back</button>
                    <button type="button" class="btn btn-primary" onclick="finishDeviceWizard()">💾 Save & Finish</button>
                </div>
            </div>
        `;
    }

    return `
        <div class="table-container" style="max-height: 65vh; overflow-y: auto;">
            <table class="table-sm table-wizard">
                <thead>
                    <tr>
                    <tr>
                        <th style="width: 50px;">#</th>
                        <th style="width: 60px;">Type</th>
                        <th style="width: 100px;">Purpose</th>
                        <th style="width: 250px;">
                            Label
                            <button type="button" class="btn btn-secondary btn-sm" style="padding: 0px 4px; font-size: 0.7rem; margin-left: 5px;" onclick="autoFillWizardLabels()" title="Suggest labels [Default Label] (Dev ID)">Auto Fill</button>
                        </th>
                        <th style="width: 80px;">Units</th>
                        <th style="width: 80px;">Scale</th>
                        <th style="width: 80px;">Offset</th>
                        <th style="width: 100px;">Visibility</th>
                        <th style="width: 120px;">Mode</th>
                        <th style="width: 80px;">Rate</th>
                        <th style="width: 50px;">Sync</th>
                    </tr>
                </thead>
                <tbody id="deviceIOBody">
                    ${configs.map((c, idx) => `
                        <tr class="io-config-row" data-idx="${idx}" data-io-def-id="${c.fk_io_definition_id}">
                            <td><small class="text-muted">${c.logical_address}</small></td>
                            <td><small>${c.io_type.substring(0,3)}</small></td>
                            <td><small>${c.purpose}</small></td>
                            <td>
                                <input type="text" class="form-control form-control-sm" value="${c.user_label || ''}" data-field="user_label">
                            </td>
                            <td>
                                <input type="text" class="form-control form-control-sm" value="${c.units || ''}" data-field="units" placeholder="-">
                            </td>
                            <td>
                                <input type="text" inputmode="decimal" class="form-control form-control-sm" value="${formatDecimal(c.scale_factor)}" data-field="scale_factor" onkeydown="onlyNumbersDotMinus(event)">
                            </td>
                            <td>
                                <input type="text" inputmode="decimal" class="form-control form-control-sm" value="${formatDecimal(c.offset)}" data-field="offset" onkeydown="onlyNumbersDotMinus(event)">
                            </td>
                            <td>
                                <select class="form-control form-control-sm" data-field="visibility">
                                    <option value="visible" ${c.visibility === 'visible' ? 'selected' : ''}>Show</option>
                                    <option value="hidden" ${c.visibility === 'hidden' ? 'selected' : ''}>Hide</option>
                                </select>
                            </td>
                            <td>
                                <select class="form-control form-control-sm" data-field="visibility_mode">
                                    <option value="periodically" ${c.visibility_mode === 'periodically' ? 'selected' : ''}>Period</option>
                                    <option value="changes" ${c.visibility_mode === 'changes' ? 'selected' : ''}>Change</option>
                                </select>
                            </td>
                            <td>
                                <input type="number" class="form-control form-control-sm" value="${c.refresh_rate}" data-field="refresh_rate" min="1" onkeydown="onlyNumbers(event)">
                            </td>
                            <td class="text-center">
                                <input type="checkbox" ${c.sync ? 'checked' : ''} data-field="sync">
                            </td>
                        </tr>
                    `).join('')}
                </tbody>
            </table>
        </div>
        <div class="wizard-actions">
            <button type="button" class="btn btn-secondary" onclick="deviceWizardBack()">← Back</button>
            <button type="button" class="btn btn-primary" onclick="finishDeviceWizard()">💾 Save & Finish</button>
        </div>
    `;
}

function attachDeviceStep2Handlers() {
    // No specific form submit, just button clicks
}

function deviceWizardBack() {
    deviceWizardState.step = 1;
    renderDeviceWizard();
}

function autoFillWizardLabels() {
    const rows = document.querySelectorAll('.io-config-row');
    const devId = deviceWizardState.deviceId ? deviceWizardState.deviceId : 'Auto';
    
    rows.forEach(row => {
        const idx = parseInt(row.dataset.idx);
        const original = deviceWizardState.ioConfig[idx];
        if (!original) return;
        
        // Use default_io_label, fallback to io_type if empty
        const defaultLabel = original.default_io_label || original.io_type || 'Point';
        const suggested = `${defaultLabel} (Dev ${devId})`;
        
        const input = row.querySelector(`[data-field="user_label"]`);
        if (input) {
            input.value = suggested;
        }
    });
}

async function finishDeviceWizard() {
    const btn = document.querySelector('.wizard-actions .btn-primary');
    const originalText = btn.textContent;
    btn.textContent = 'Saving...';
    btn.disabled = true;

    try {
        const payload = { ...deviceWizardState.step1Data };

        if (deviceWizardState.mode === 'create') {
            // =================================================================
            // CREATE MODE: Send device + all IO config in a single request.
            // The backend handles this in one transaction — if anything fails,
            // nothing is saved and we get the exact error back.
            // =================================================================

            // Collect current IO config from the wizard Step 2 form
            const rows = document.querySelectorAll('.io-config-row');
            const ioConfig = [];

            rows.forEach(row => {
                const idx = parseInt(row.dataset.idx);
                const original = deviceWizardState.ioConfig[idx];
                if (!original) return;

                const getVal = (f) => row.querySelector(`[data-field="${f}"]`).value;
                const getCheck = (f) => row.querySelector(`[data-field="${f}"]`).checked;

                ioConfig.push({
                    fk_io_definition_id: original.fk_io_definition_id,
                    user_label: getVal('user_label'),
                    units: getVal('units') || null,
                    scale_factor: parseDecimal(getVal('scale_factor')),
                    offset: parseDecimal(getVal('offset')),
                    visibility: getVal('visibility'),
                    visibility_mode: getVal('visibility_mode'),
                    refresh_rate: parseInt(getVal('refresh_rate')),
                    sync: getCheck('sync')
                });
            });

            // Client-side validation: check for empty labels
            const emptyLabel = ioConfig.find(io => !io.user_label || io.user_label.trim() === '');
            if (emptyLabel) {
                showToast('All I/O points must have a label. Please fill in all empty labels.', 'error');
                btn.textContent = originalText;
                btn.disabled = false;
                return;
            }

            // Client-side validation: check for duplicate labels
            const labels = ioConfig.map(io => io.user_label);
            const duplicates = labels.filter((label, i) => labels.indexOf(label) !== i);
            if (duplicates.length > 0) {
                const uniqueDuplicates = [...new Set(duplicates)];
                showToast(`Duplicate labels found: ${uniqueDuplicates.join(', ')}. Each label must be unique.`, 'error');
                btn.textContent = originalText;
                btn.disabled = false;
                return;
            }

            // Send everything in one request
            payload.io_config = ioConfig;

            const res = await fetch('/api/devices', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const result = await res.json();
            if (!res.ok) {
                // Show exact error from the server — wizard stays open
                showToast(result.error || 'Failed to create device', 'error');
                btn.textContent = originalText;
                btn.disabled = false;
                return;
            }

            showToast('Device created successfully', 'success');
            loadDevices();
            closeModal();

        } else {
            // =================================================================
            // EDIT MODE: Update device. 
            // If model changed, replace all IO configs. 
            // If not, apply changed IO configs individually.
            // =================================================================
            const preSaveConfig = [...deviceWizardState.ioConfig];

            if (deviceWizardState.modelChanged) {
                // Collect full new IO config
                const rows = document.querySelectorAll('.io-config-row');
                const ioConfig = [];

                rows.forEach(row => {
                    const idx = parseInt(row.dataset.idx);
                    const original = deviceWizardState.ioConfig[idx];
                    if (!original) return;

                    const getVal = (f) => row.querySelector(`[data-field="${f}"]`).value;
                    const getCheck = (f) => row.querySelector(`[data-field="${f}"]`).checked;

                    ioConfig.push({
                        fk_io_definition_id: original.fk_io_definition_id,
                        user_label: getVal('user_label'),
                        units: getVal('units') || null,
                        scale_factor: parseDecimal(getVal('scale_factor')),
                        offset: parseDecimal(getVal('offset')),
                        visibility: getVal('visibility'),
                        visibility_mode: getVal('visibility_mode'),
                        refresh_rate: parseInt(getVal('refresh_rate')),
                        sync: getCheck('sync')
                    });
                });

                // Validation
                const emptyLabel = ioConfig.find(io => !io.user_label || io.user_label.trim() === '');
                if (emptyLabel) {
                    showToast('All I/O points must have a label. Please fill in all empty labels.', 'error');
                    btn.textContent = originalText;
                    btn.disabled = false;
                    return;
                }
                const labels = ioConfig.map(io => io.user_label);
                const duplicates = labels.filter((label, i) => labels.indexOf(label) !== i);
                if (duplicates.length > 0) {
                    const uniqueDuplicates = [...new Set(duplicates)];
                    showToast(`Duplicate labels found: ${uniqueDuplicates.join(', ')}. Each label must be unique.`, 'error');
                    btn.textContent = originalText;
                    btn.disabled = false;
                    return;
                }

                payload.io_config = ioConfig;
            }

            const res = await fetch(`/api/devices/${deviceWizardState.deviceId}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            if (!res.ok) {
                const result = await res.json();
                showToast(result.error || 'Failed to update device', 'error');
                btn.textContent = originalText;
                btn.disabled = false;
                return;
            }

            let updatesCount = 0;

            if (!deviceWizardState.modelChanged) {
                // Collect and apply IO config changes individually (dirty check)
                const rows = document.querySelectorAll('.io-config-row');
                const updates = [];

                rows.forEach(row => {
                    const idx = parseInt(row.dataset.idx);
                    const original = preSaveConfig[idx];
                    if (!original) return;

                    const getVal = (f) => row.querySelector(`[data-field="${f}"]`).value;
                    const getCheck = (f) => row.querySelector(`[data-field="${f}"]`).checked;

                    const newVal = {
                        user_label: getVal('user_label'),
                        units: getVal('units') || null,
                        scale_factor: parseDecimal(getVal('scale_factor')),
                        offset: parseDecimal(getVal('offset')),
                        visibility: getVal('visibility'),
                        visibility_mode: getVal('visibility_mode'),
                        refresh_rate: parseInt(getVal('refresh_rate')),
                        sync: getCheck('sync')
                    };

                    // Dirty check
                    if (newVal.user_label !== original.user_label ||
                        newVal.units !== original.units ||
                        newVal.scale_factor !== parseDecimal(formatDecimal(original.scale_factor)) ||
                        newVal.offset !== parseDecimal(formatDecimal(original.offset)) ||
                        newVal.visibility !== original.visibility ||
                        newVal.visibility_mode !== original.visibility_mode ||
                        newVal.refresh_rate !== parseInt(original.refresh_rate) ||
                        newVal.sync !== Boolean(original.sync)) {

                        updates.push({
                            fk_io_definition_id: original.fk_io_definition_id,
                            ...newVal
                        });
                    }
                });

                if (updates.length > 0) {
                    updatesCount = updates.length;
                    for (const u of updates) {
                        const itemResp = await fetch(`/api/module-io-config/${deviceWizardState.deviceId}/${u.fk_io_definition_id}`, {
                            method: 'PUT',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify(u)
                        });
                        if (!itemResp.ok) {
                            const r = await itemResp.json();
                            showToast(r.error || `Failed to update I/O config for ID ${u.fk_io_definition_id}`, 'error');
                            btn.textContent = originalText;
                            btn.disabled = false;
                            return;
                        }
                    }
                }
            } else {
                updatesCount = payload.io_config.length;
            }

            showToast(`Device ${updatesCount > 0 ? 'saved with ' + updatesCount + ' I/O update(s)' : 'saved successfully'}`, 'success');
            loadDevices();
            closeModal();
        }
    } catch (error) {
        console.error(error);
        showToast('Failed to save device: ' + error.message, 'error');
        btn.textContent = originalText;
        btn.disabled = false;
    }
}



async function deleteDevice(id) {
    if (!confirm('Are you sure you want to delete this device? This will also delete all associated I/O configurations.')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/devices/${id}`, { method: 'DELETE' });
        
        if (response.ok) {
            showToast('Device deleted successfully', 'success');
            loadDevices();
        } else {
            const result = await response.json();
            showToast(result.error || 'Failed to delete device', 'error');
        }
    } catch (error) {
        showToast('Failed to delete device', 'error');
    }
}

// =============================================================================
// I/O DEFINITIONS
// =============================================================================

async function loadIODefinitions() {
    try {
        const modelsResponse = await fetch('/api/models');
        models = await modelsResponse.json();
        
        const response = await fetch('/api/io-definitions');
        const definitions = await response.json();
        
        const content = document.getElementById('contentArea');
        
        // Group by model
        const byModel = definitions.reduce((acc, def) => {
            const modelName = def.model_name || 'Unknown';
            if (!acc[modelName]) acc[modelName] = [];
            acc[modelName].push(def);
            return acc;
        }, {});
        
        if (Object.keys(byModel).length === 0) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📍</div>
                    <p>No I/O definitions found</p>
                </div>
            `;
            return;
        }
        
        let html = '';
        for (const [modelName, defs] of Object.entries(byModel)) {
            html += `
                <div class="card">
                    <div class="card-title">🔧 ${modelName}</div>
                    <div class="table-container">
                        <table>
                            <thead>
                                <tr>
                                    <th>Logical Addr</th>
                                    <th>Type</th>
                                    <th>Purpose</th>
                                    <th>Access</th>
                                    <th>Physical Addr</th>
                                    <th>Method</th>
                                    <th>Label</th>
                                    <th>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                ${defs.map(d => {
                                    const type = String(d.io_type || '').trim().toLowerCase();
                                    const purpose = String(d.purpose || '').trim().toLowerCase();
                                    const access = String(d.hardware_access || '').trim().toLowerCase();
                                    
                                    // Debug logging
                                    if (Math.random() < 0.01) console.log('IODef item:', { type, purpose, access, raw: d });

                                    return `
                                    <tr>
                                        <td>${d.logical_address}</td>
                                        <td><span class="badge ${type === 'bit' ? 'badge-bit' : 'badge-register'}">${d.io_type}</span></td>
                                        <td><span class="badge ${purpose === 'standard' ? 'badge-standard' : purpose === 'secure_state' ? 'badge-secure' : 'badge-config'}">${d.purpose}</span></td>
                                        <td><span class="badge ${access === 'readwrite' ? 'badge-readwrite' : 'badge-readonly'}">${d.hardware_access}</span></td>
                                        <td>${d.physical_address}</td>
                                        <td>${d.access_method}${d.bitmask_offset !== null ? `:${d.bitmask_offset}` : ''}</td>
                                        <td>${d.default_io_label || '-'}</td>
                                        <td class="table-actions">
                                            <button class="btn btn-secondary btn-sm btn-icon" onclick="editIODef(${d.io_definition_id})" title="Edit">✏️</button>
                                            <button class="btn btn-danger btn-sm btn-icon" onclick="deleteIODef(${d.io_definition_id})" title="Delete">🗑️</button>
                                        </td>
                                    </tr>
                                    `;
                                }).join('')}
                            </tbody>
                        </table>
                    </div>
                </div>
            `;
        }
        
        content.innerHTML = html;
    } catch (error) {
        showToast('Failed to load I/O definitions', 'error');
    }
}

function showCreateIODefForm() {
    const modelOptions = models.map(m => `<option value="${m.model_id}">${m.model_name}</option>`).join('');
    
    showModal('Create I/O Definition', `
        <form id="ioDefForm">
            <div class="form-group">
                <label for="fk_model_id">Model *</label>
                <select id="fk_model_id" class="form-control" required>
                    <option value="">-- Select Model --</option>
                    ${modelOptions}
                </select>
            </div>
            <div class="form-row">
                <div class="form-group">
                    <label for="logical_address">Logical Address *</label>
                    <input type="number" id="logical_address" class="form-control" required value="0" onkeydown="onlyNumbers(event)">
                </div>
                <div class="form-group">
                    <label for="physical_address">Physical Address</label>
                    <input type="number" id="physical_address" class="form-control" value="0" onkeydown="onlyNumbers(event)">
                </div>
            </div>
            <div class="form-row">
                <div class="form-group">
                    <label for="io_type">I/O Type *</label>
                    <select id="io_type" class="form-control" required>
                        <option value="bit">Bit</option>
                        <option value="register">Register</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="purpose">Purpose *</label>
                    <select id="purpose" class="form-control" required>
                        <option value="standard">Standard</option>
                        <option value="secure_state">Secure State</option>
                        <option value="config">Config</option>
                    </select>
                </div>
            </div>
            <div class="form-row">
                <div class="form-group">
                    <label for="hardware_access">Hardware Access *</label>
                    <select id="hardware_access" class="form-control" required>
                        <option value="readonly">Read Only</option>
                        <option value="readwrite">Read/Write</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="access_method">Access Method</label>
                    <select id="access_method" class="form-control">
                        <option value="direct">Direct</option>
                        <option value="bitmask">Bitmask</option>
                    </select>
                </div>
            </div>
            <div class="form-row">
                <div class="form-group">
                    <label for="bitmask_offset">Bitmask Offset</label>
                    <input type="number" id="bitmask_offset" class="form-control" placeholder="Optional" onkeydown="onlyNumbers(event)">
                </div>
                <div class="form-group">
                    <label for="register_count">Register Count</label>
                    <input type="number" id="register_count" class="form-control" value="1" onkeydown="onlyNumbers(event)">
                </div>
            </div>
            <div class="form-row">
                <div class="form-group">
                    <label for="endianess">Endianess</label>
                    <select id="endianess" class="form-control">
                        <option value="big">Big</option>
                        <option value="little">Little</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="default_io_label">Default Label</label>
                    <input type="text" id="default_io_label" class="form-control" placeholder="e.g., Output 1">
                </div>
            </div>
            <div class="form-actions">
                <button type="button" class="btn btn-secondary" onclick="closeModal()">Cancel</button>
                <button type="submit" class="btn btn-success">Create</button>
            </div>
        </form>
    `);
    
    document.getElementById('ioDefForm').onsubmit = async (e) => {
        e.preventDefault();
        await createIODef();
    };
}

async function createIODef() {
    const bitmaskOffset = document.getElementById('bitmask_offset').value;

    // Validate numeric fields before building data
    const logAddr = validateInt(document.getElementById('logical_address').value, 'Logical address', {required: true, min: 0, max: 65535});
    if (!logAddr.valid) { showToast(logAddr.error, 'error'); return; }
    const physAddr = validateInt(document.getElementById('physical_address').value, 'Physical address', {required: true, min: 0, max: 65535});
    if (!physAddr.valid) { showToast(physAddr.error, 'error'); return; }
    const regCount = validateInt(document.getElementById('register_count').value, 'Register count', {min: 1, max: 4});
    if (!regCount.valid) { showToast(regCount.error, 'error'); return; }
    if (bitmaskOffset !== '') {
        const bmOff = validateInt(bitmaskOffset, 'Bitmask offset', {min: 0, max: 15});
        if (!bmOff.valid) { showToast(bmOff.error, 'error'); return; }
    }

    const data = {
        fk_model_id: parseInt(document.getElementById('fk_model_id').value),
        logical_address: logAddr.value,
        io_type: document.getElementById('io_type').value,
        purpose: document.getElementById('purpose').value,
        hardware_access: document.getElementById('hardware_access').value,
        physical_address: physAddr.value,
        access_method: document.getElementById('access_method').value,
        bitmask_offset: bitmaskOffset ? parseInt(bitmaskOffset) : null,
        register_count: regCount.value || 1,
        endianess: document.getElementById('endianess').value,
        default_io_label: document.getElementById('default_io_label').value || null
    };
    
    try {
        const response = await fetch('/api/io-definitions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        
        const result = await response.json();
        
        if (response.ok) {
            showToast('I/O Definition created successfully', 'success');
            closeModal();
            loadIODefinitions();
        } else {
            showToast(result.error || 'Failed to create I/O definition', 'error');
        }
    } catch (error) {
        showToast('Failed to create I/O definition', 'error');
    }
}

async function deleteIODef(id) {
    if (!confirm('Are you sure you want to delete this I/O definition?')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/io-definitions/${id}`, { method: 'DELETE' });
        
        if (response.ok) {
            showToast('I/O Definition deleted successfully', 'success');
            loadIODefinitions();
        } else {
            const result = await response.json();
            showToast(result.error || 'Failed to delete I/O definition', 'error');
        }
    } catch (error) {
        showToast('Failed to delete I/O definition', 'error');
    }
}

async function editIODef(id) {
    showToast('Edit functionality coming soon', 'info');
}

// =============================================================================
// MODULE I/O CONFIG
// =============================================================================

async function loadModuleIOConfig() {
    try {
        const devicesResponse = await fetch('/api/devices');
        devices = await devicesResponse.json();
        
        const content = document.getElementById('contentArea');
        
        if (devices.length === 0) {
            content.innerHTML = `<div class="empty-state"><div class="empty-state-icon">🏷️</div><p>No devices available</p></div>`;
            return;
        }
        
        // Show device selector
        const deviceOptions = devices.map(d => `<option value="${d.module_id}">${d.module_name}</option>`).join('');
        
        content.innerHTML = `
            <div class="filters">
                <div class="filter-group">
                    <label>Select Device:</label>
                    <select id="deviceSelector" class="form-control" onchange="loadModuleIOForDevice(this.value)">
                        <option value="">-- Select Device --</option>
                        ${deviceOptions}
                    </select>
                </div>
            </div>
            <div id="moduleIOContent"></div>
        `;
    } catch (error) {
        showToast('Failed to load devices', 'error');
    }
}

async function loadModuleIOForDevice(moduleId) {
    if (!moduleId) {
        document.getElementById('moduleIOContent').innerHTML = '';
        return;
    }
    
    try {
        const response = await fetch(`/api/module-io-config?module_id=${moduleId}`);
        const configs = await response.json();
        
        const container = document.getElementById('moduleIOContent');
        
        if (configs.length === 0) {
            container.innerHTML = `<div class="empty-state"><p>No I/O config for this device</p></div>`;
            return;
        }
        
        container.innerHTML = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>Address</th>
                            <th>Type</th>
                            <th>Purpose</th>
                            <th>Label</th>
                            <th>Scale</th>
                            <th>Offset</th>
                            <th>Visibility</th>
                            <th>Sync</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${configs.map(c => `
                            <tr>
                                <td>${c.logical_address}</td>
                                <td><span class="badge ${String(c.io_type).toLowerCase() === 'bit' ? 'badge-bit' : 'badge-register'}">${c.io_type}</span></td>
                                <td><span class="badge ${String(c.purpose).toLowerCase() === 'standard' ? 'badge-standard' : String(c.purpose).toLowerCase() === 'secure_state' ? 'badge-secure' : 'badge-config'}">${c.purpose}</span></td>
                                <td>${c.user_label}</td>
                                <td>${c.scale_factor}</td>
                                <td>${c.offset}</td>
                                <td><span class="badge ${c.visibility === 'visible' ? 'badge-success' : 'badge-warning'}">${c.visibility}</span></td>
                                <td><span class="badge ${c.sync ? 'badge-success' : 'badge-danger'}">${c.sync ? 'Yes' : 'No'}</span></td>
                                <td class="table-actions">
                                    <button class="btn btn-secondary btn-sm btn-icon" onclick="editModuleIOConfig(${moduleId}, ${c.fk_io_definition_id})" title="Edit">✏️</button>
                                </td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
    } catch (error) {
        showToast('Failed to load module I/O config', 'error');
    }
}

async function editModuleIOConfig(moduleId, ioDefId) {
    showToast('Edit functionality coming soon', 'info');
}

// =============================================================================
// SECURE STATE MAPPING
// =============================================================================

async function loadSecureStateMapping() {
    try {
        const response = await fetch('/api/secure-state-mapping');
        const mappings = await response.json();
        
        const content = document.getElementById('contentArea');
        
        if (mappings.length === 0) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">🔒</div>
                    <p>No secure state mappings defined</p>
                </div>
            `;
            return;
        }
        
        content.innerHTML = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>Model</th>
                            <th>Standard I/O</th>
                            <th>Secure State I/O</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${mappings.map(m => `
                            <tr>
                                <td>${m.model_name}</td>
                                <td>[${m.standard_address}] ${m.standard_label || '-'}</td>
                                <td>[${m.secure_address}] ${m.secure_label || '-'}</td>
                                <td class="table-actions">
                                    <button class="btn btn-danger btn-sm btn-icon" onclick="deleteSecureStateMapping(${m.fk_model_id}, ${m.fk_standard_io_definition_id})" title="Delete">🗑️</button>
                                </td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
    } catch (error) {
        showToast('Failed to load secure state mappings', 'error');
    }
}

function showCreateSecureStateForm() {
    showToast('Create secure state mapping form coming soon', 'info');
}

async function deleteSecureStateMapping(modelId, standardId) {
    if (!confirm('Delete this secure state mapping?')) return;
    
    try {
        const response = await fetch(`/api/secure-state-mapping/${modelId}/${standardId}`, { method: 'DELETE' });
        if (response.ok) {
            showToast('Mapping deleted', 'success');
            loadSecureStateMapping();
        } else {
            showToast('Failed to delete mapping', 'error');
        }
    } catch (error) {
        showToast('Failed to delete mapping', 'error');
    }
}

// =============================================================================
// AGGREGATED I/O MAPPING
// =============================================================================

async function loadAggregatedIOMap() {
    try {
        const response = await fetch('/api/aggregated-io-map');
        const mappings = await response.json();
        
        const content = document.getElementById('contentArea');
        
        if (mappings.length === 0) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">🔀</div>
                    <p>No aggregated I/O mappings defined</p>
                </div>
            `;
            return;
        }
        
        content.innerHTML = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Aggregated I/O</th>
                            <th>→</th>
                            <th>Child Module</th>
                            <th>Child I/O</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${mappings.map(m => `
                            <tr>
                                <td>${m.map_id}</td>
                                <td>[${m.aggregated_address}] ${m.aggregated_label || '-'}</td>
                                <td>→</td>
                                <td>${m.child_module_name}</td>
                                <td>[${m.child_address}] ${m.child_label || '-'}</td>
                                <td class="table-actions">
                                    <button class="btn btn-danger btn-sm btn-icon" onclick="deleteAggregatedIOMap(${m.map_id})" title="Delete">🗑️</button>
                                </td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
    } catch (error) {
        showToast('Failed to load aggregated I/O mappings', 'error');
    }
}

function showCreateAggregatedIOForm() {
    showToast('Create aggregated I/O mapping form coming soon', 'info');
}

async function deleteAggregatedIOMap(mapId) {
    if (!confirm('Delete this aggregated I/O mapping?')) return;
    
    try {
        const response = await fetch(`/api/aggregated-io-map/${mapId}`, { method: 'DELETE' });
        if (response.ok) {
            showToast('Mapping deleted', 'success');
            loadAggregatedIOMap();
        } else {
            showToast('Failed to delete mapping', 'error');
        }
    } catch (error) {
        showToast('Failed to delete mapping', 'error');
    }
}

// =============================================================================
// PLC SETTINGS
// =============================================================================

async function loadPLCSettings() {
    try {
        const response = await fetch('/api/plc-settings');
        const settings = await response.json();
        
        const content = document.getElementById('contentArea');
        content.innerHTML = `
            <div class="card">
                <div class="card-title">⚙️ PLC Configuration</div>
                <form id="plcSettingsForm">
                    <div class="form-row">
                        <div class="form-group">
                            <label for="rs485_baud_rate">RS-485 Baud Rate</label>
                            <select id="rs485_baud_rate" class="form-control" ${!canWrite() ? 'disabled' : ''}>
                                <option value="9600" ${settings.rs485_baud_rate === 9600 ? 'selected' : ''}>9600</option>
                                <option value="19200" ${settings.rs485_baud_rate === 19200 ? 'selected' : ''}>19200</option>
                                <option value="38400" ${settings.rs485_baud_rate === 38400 ? 'selected' : ''}>38400</option>
                                <option value="57600" ${settings.rs485_baud_rate === 57600 ? 'selected' : ''}>57600</option>
                                <option value="115200" ${settings.rs485_baud_rate === 115200 ? 'selected' : ''}>115200</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label for="rs485_data_bits">Data Bits</label>
                            <select id="rs485_data_bits" class="form-control" ${!canWrite() ? 'disabled' : ''}>
                                <option value="7" ${settings.rs485_data_bits === 7 ? 'selected' : ''}>7</option>
                                <option value="8" ${settings.rs485_data_bits === 8 ? 'selected' : ''}>8</option>
                            </select>
                        </div>
                    </div>
                    <div class="form-row">
                        <div class="form-group">
                            <label for="rs485_parity">Parity</label>
                            <select id="rs485_parity" class="form-control" ${!canWrite() ? 'disabled' : ''}>
                                <option value="N" ${settings.rs485_parity === 'N' ? 'selected' : ''}>None</option>
                                <option value="E" ${settings.rs485_parity === 'E' ? 'selected' : ''}>Even</option>
                                <option value="O" ${settings.rs485_parity === 'O' ? 'selected' : ''}>Odd</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label for="rs485_stop_bits">Stop Bits</label>
                            <select id="rs485_stop_bits" class="form-control" ${!canWrite() ? 'disabled' : ''}>
                                <option value="1" ${settings.rs485_stop_bits === 1 ? 'selected' : ''}>1</option>
                                <option value="2" ${settings.rs485_stop_bits === 2 ? 'selected' : ''}>2</option>
                            </select>
                        </div>
                    </div>
                    <div class="form-group">
                        <label for="operation_mode">Operation Mode</label>
                        <select id="operation_mode" class="form-control" ${!canWrite() ? 'disabled' : ''}>
                            <option value="execution" ${settings.operation_mode === 'execution' ? 'selected' : ''}>Execution</option>
                            <option value="configuration" ${settings.operation_mode === 'configuration' ? 'selected' : ''}>Configuration</option>
                        </select>
                    </div>
                    ${canWrite() ? `
                    <div class="form-actions">
                        <button type="submit" class="btn btn-success">💾 Save Settings</button>
                    </div>
                    ` : '<div class="alert alert-info">You do not have permission to modify PLC settings.</div>'}
                </form>
            </div>
        `;
        
        document.getElementById('plcSettingsForm').onsubmit = async (e) => {
            e.preventDefault();
            await savePLCSettings();
        };
    } catch (error) {
        showToast('Failed to load PLC settings', 'error');
    }
}

async function savePLCSettings() {
    // Validate numeric fields
    const baudRate = validateInt(document.getElementById('rs485_baud_rate').value, 'Baud rate', 
        {required: true, choices: [9600, 19200, 38400, 57600, 115200]});
    if (!baudRate.valid) { showToast(baudRate.error, 'error'); return; }
    const dataBits = validateInt(document.getElementById('rs485_data_bits').value, 'Data bits',
        {required: true, choices: [7, 8]});
    if (!dataBits.valid) { showToast(dataBits.error, 'error'); return; }
    const stopBits = validateInt(document.getElementById('rs485_stop_bits').value, 'Stop bits',
        {required: true, choices: [1, 2]});
    if (!stopBits.valid) { showToast(stopBits.error, 'error'); return; }

    const parity = document.getElementById('rs485_parity').value;
    if (!['N', 'E', 'O'].includes(parity)) {
        showToast('Parity must be N, E, or O', 'error');
        return;
    }

    const data = {
        rs485_baud_rate: baudRate.value,
        rs485_data_bits: dataBits.value,
        rs485_parity: parity,
        rs485_stop_bits: stopBits.value,
        operation_mode: document.getElementById('operation_mode').value
    };
    
    try {
        const response = await fetch('/api/plc-settings', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        
        if (response.ok) {
            showToast('Settings saved successfully', 'success');
        } else {
            const result = await response.json();
            showToast(result.error || 'Failed to save settings', 'error');
        }
    } catch (error) {
        showToast('Failed to save settings', 'error');
    }
}

// =============================================================================
// GUI CONFIG
// =============================================================================

function loadGUIConfig() {
    const content = document.getElementById('contentArea');
    content.innerHTML = `
        <div class="card" style="max-width: 600px;">
            <div class="card-title">🖥️ Interface Settings</div>
            <p class="form-hint">Configure how data is displayed and entered in the web interface.</p>
            
            <div class="form-group" style="margin-top: 20px;">
                <label for="decimal_separator">Decimal Separator</label>
                <select id="decimal_separator" class="form-control" onchange="updateDecimalSeparator(this.value)">
                    <option value="." ${guiConfig.decimalSeparator === '.' ? 'selected' : ''}>Point (.) - e.g. 1.23</option>
                    <option value="," ${guiConfig.decimalSeparator === ',' ? 'selected' : ''}>Comma (,) - e.g. 1,23</option>
                </select>
                <span class="form-hint">Affects Real-Time Twin values, Scale factors, and Offsets.</span>
            </div>
            
            <div class="alert alert-info" style="margin-top: 20px; padding: 12px; border-left: 4px solid var(--accent); background: rgba(var(--accent-rgb), 0.1);">
                <strong>Note:</strong> This setting is stored locally in your browser (LocalStorage). Internal data processing (API) still uses standard numeric formats.
            </div>
        </div>
    `;
}

function updateDecimalSeparator(value) {
    guiConfig.decimalSeparator = value;
    localStorage.setItem('decimalSeparator', value);
    showToast(`Decimal separator updated to "${value}"`, 'success');
}

// =============================================================================
// RTMIRROR (Real-Time Twin) - Auto-refresh every 200ms
// =============================================================================

let rtMirrorIntervalId = null;
let rtMirrorCurrentModuleId = '';
let rtMirrorLastModuleId = null;

async function loadRTMirror() {
    // Stop any existing interval
    stopRTMirrorAutoRefresh();
    
    try {
        const devicesResponse = await fetch('/api/devices');
        devices = await devicesResponse.json();
        
        const content = document.getElementById('contentArea');
        
        if (devices.length === 0) {
            content.innerHTML = `<div class="empty-state"><div class="empty-state-icon">📊</div><p>No devices available</p></div>`;
            return;
        }
        
        const deviceOptions = devices.map(d => `<option value="${d.module_id}">${d.module_name}</option>`).join('');
        
        content.innerHTML = `
            <div class="filters">
                <div class="filter-group">
                    <label>Select Device:</label>
                    <select id="rtDeviceSelector" class="form-control" onchange="changeRTMirrorDevice(this.value)">
                        <option value="">-- All Devices --</option>
                        ${deviceOptions}
                    </select>
                </div>
                <div class="status-indicator">
                    <span class="pulse-dot"></span>
                    <span>Live (200ms)</span>
                </div>
            </div>
            <div id="rtMirrorContent"></div>
        `;
        
        // Initial load
        rtMirrorCurrentModuleId = '';
        await refreshRTMirrorData();
        
        // Start auto-refresh
        startRTMirrorAutoRefresh();
    } catch (error) {
        showToast('Failed to load devices', 'error');
    }
}

function changeRTMirrorDevice(moduleId) {
    rtMirrorCurrentModuleId = moduleId;
    // Force clear content to reset the incremental update logic and show loading
    const container = document.getElementById('rtMirrorContent');
    if (container) container.innerHTML = '<div class="loading"><div class="spinner"></div>Loading...</div>';
    refreshRTMirrorData();
}

function startRTMirrorAutoRefresh() {
    if (rtMirrorIntervalId) return;
    rtMirrorIntervalId = setInterval(refreshRTMirrorData, 200);
}

function stopRTMirrorAutoRefresh() {
    if (rtMirrorIntervalId) {
        clearInterval(rtMirrorIntervalId);
        rtMirrorIntervalId = null;
    }
}

async function refreshRTMirrorData() {
    // Only refresh if we're on the rtmirror page
    if (currentPage !== 'rtmirror') {
        stopRTMirrorAutoRefresh();
        return;
    }
    
    const targetModuleId = rtMirrorCurrentModuleId;
    try {
        const url = targetModuleId 
            ? `/api/rtmirror?module_id=${targetModuleId}` 
            : '/api/rtmirror';
        const response = await fetch(url);
        const data = await response.json();
        
        // RACE CONDITION PROTECTION: If the user changed the device while we were fetching, ignore this data
        if (targetModuleId !== rtMirrorCurrentModuleId) return;
        
        const container = document.getElementById('rtMirrorContent');
        if (!container) {
            stopRTMirrorAutoRefresh();
            return;
        }
        
        if (data.length === 0) {
            container.innerHTML = `<div class="empty-state"><p>No real-time data available</p></div>`;
            return;
        }
        
        // Check if table already exists for incremental update
        const existingTable = container.querySelector('table');
        const existingTbody = existingTable ? existingTable.querySelector('tbody') : null;
        
        if (existingTbody && existingTbody.rows.length === data.length && rtMirrorCurrentModuleId === rtMirrorLastModuleId) {
            // --- INCREMENTAL UPDATE: only update read-only cells, leave inputs untouched ---
            for (let i = 0; i < data.length; i++) {
                const r = data[i];
                const row = existingTbody.rows[i];
                if (!row) continue;
                
                const cells = row.cells;
                // Cell indices: 0=Device, 1=Address, 2=Label, 3=Unit, 4=Type, 5=Access, 6=Purpose, 7=Value, 8=NetValue, 9=Required(input), 10=NetRequired(input), 11=Timestamp
                
                // Update read-only value cells
                cells[7].textContent = formatDecimal(r.value);
                cells[8].textContent = formatDecimal(r.net_value !== null ? r.net_value.toFixed(2) : null);
                
                // Update timestamp
                cells[11].textContent = r.timestamp ? new Date(r.timestamp).toLocaleTimeString() : '-';
                
                // Update input values ONLY if the input is NOT currently focused
                const reqInput = cells[9].querySelector('input');
                const netReqInput = cells[10].querySelector('input');
                
                if (reqInput && document.activeElement !== reqInput) {
                    const newReqVal = formatDecimal(r.required_value);
                    if (reqInput.value !== String(newReqVal)) {
                        reqInput.value = newReqVal;
                    }
                }
                if (netReqInput && document.activeElement !== netReqInput) {
                    const newNetReqVal = formatDecimal(r.net_required_value);
                    if (netReqInput.value !== String(newNetReqVal)) {
                        netReqInput.value = newNetReqVal;
                    }
                }
            }
            return;
        }
        
        // --- FULL RENDER: first time or row count changed ---
        const rowsHtml = data.map(r => {
            const isReadOnly = String(r.hardware_access).toLowerCase() === 'readonly';
            const isBit = r.io_type === 'bit';
            const moduleId = r.fk_module_id;
            const ioDefId = r.fk_io_definition_id;
            
            const reqInputId = `req_${moduleId}_${ioDefId}`;
            const netReqInputId = `netreq_${moduleId}_${ioDefId}`;
            
            const reqValue = formatDecimal(r.required_value);
            const netReqValue = formatDecimal(r.net_required_value);
            
            const inputType = isBit ? 'number' : 'text';
            const inputMode = isBit ? '' : 'inputmode="decimal"';
            const minMax = isBit ? 'min="0" max="1" step="1"' : '';
            const placeholder = isBit ? '0/1' : 'value';
            
            const shouldDisable = isReadOnly || !canWrite();
            const disabledClass = shouldDisable ? 'rt-input-disabled' : 'rt-input';
            const disabled = shouldDisable ? 'disabled' : '';
            
            return `
                <tr class="row-output">
                    <td>${r.module_name}</td>
                    <td>${r.logical_address}</td>
                    <td>${r.user_label || '-'}</td>
                    <td>${r.units || '-'}</td>
                    <td><span class="badge ${isBit ? 'badge-bit' : 'badge-register'}">${r.io_type}</span></td>
                    <td><span class="badge ${isReadOnly ? 'badge-readonly' : 'badge-readwrite'}">${r.hardware_access}</span></td>
                    <td><span class="badge ${String(r.purpose).toLowerCase() === 'standard' ? 'badge-standard' : String(r.purpose).toLowerCase() === 'secure_state' ? 'badge-secure' : 'badge-config'}">${r.purpose}</span></td>
                    <td class="value-cell">${formatDecimal(r.value)}</td>
                    <td class="value-cell">${formatDecimal(r.net_value !== null ? r.net_value.toFixed(2) : null)}</td>
                    <td>
                        <input type="${inputType}" 
                               ${inputMode}
                               id="${reqInputId}" 
                               class="${disabledClass}" 
                               value="${reqValue}" 
                               placeholder="${placeholder}"
                               ${minMax}
                               ${disabled}
                               data-module="${moduleId}"
                               data-iodef="${ioDefId}"
                               data-field="required_value"
                               data-isbit="${isBit}"
                               onkeydown="handleRTInputKeydown(event, this); onlyNumbersDotMinus(event)">
                    </td>
                    <td>
                        <input type="${inputType}" 
                               ${inputMode}
                               id="${netReqInputId}" 
                               class="${disabledClass}" 
                               value="${netReqValue}" 
                               placeholder="${placeholder}"
                               ${minMax}
                               ${disabled}
                               data-module="${moduleId}"
                               data-iodef="${ioDefId}"
                               data-field="net_required_value"
                               data-isbit="${isBit}"
                               onkeydown="handleRTInputKeydown(event, this); onlyNumbersDotMinus(event)">
                    </td>
                    <td class="text-muted small">${r.timestamp ? new Date(r.timestamp).toLocaleTimeString() : '-'}</td>
                </tr>
            `;
        }).join('');
        
        container.innerHTML = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>Device</th>
                            <th>Address</th>
                            <th>Label</th>
                            <th>Unit</th>
                            <th>Type</th>
                            <th>Access</th>
                            <th>Purpose</th>
                            <th>Value</th>
                            <th>Net Value</th>
                            <th>Required</th>
                            <th>Net Required</th>
                            <th>Updated</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${rowsHtml}
                    </tbody>
                </table>
            </div>
        `;
        
        rtMirrorLastModuleId = rtMirrorCurrentModuleId;
    } catch (error) {
        // Silent fail for auto-refresh to avoid spamming toasts
        console.error('Failed to refresh real-time data:', error);
    }
}

// Handle Enter key for RT inputs
function handleRTInputKeydown(event, input) {
    if (event.key === 'Enter') {
        event.preventDefault();
        updateRTMirrorValue(input);
        input.blur();
    }
}

// Update rtmirror value via API
async function updateRTMirrorValue(input) {
    const moduleId = input.dataset.module;
    const ioDefId = input.dataset.iodef;
    const field = input.dataset.field;
    const isBit = input.dataset.isbit === 'true';
    let value = input.value.trim();
    
    // Validate value
    if (value === '') {
        return; // Empty value, no update
    }
    
    value = parseDecimal(value);
    if (value === null || isNaN(value)) {
        showToast('Invalid value', 'error');
        return;
    }
    
    // For bits, validate 0 or 1
    if (isBit && (value !== 0 && value !== 1)) {
        showToast('Bit value must be 0 or 1', 'error');
        input.value = '';
        return;
    }
    
    // For bits, ensure integer
    if (isBit) {
        value = Math.round(value);
    }
    
    try {
        const response = await fetch(`/api/rtmirror/${moduleId}/${ioDefId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ [field]: value })
        });
        
        const result = await response.json();
        
        if (response.ok) {
            input.classList.add('input-success');
            setTimeout(() => input.classList.remove('input-success'), 500);
        } else {
            showToast(result.error || 'Failed to update value', 'error');
            input.classList.add('input-error');
            setTimeout(() => input.classList.remove('input-error'), 500);
        }
    } catch (error) {
        console.error('Failed to update value:', error);
        showToast('Failed to update value', 'error');
    }
}

// =============================================================================
// USER MANAGEMENT (Admin only)
// =============================================================================

async function loadUsers() {
    if (!isAdmin()) {
        document.getElementById('contentArea').innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">🔒</div>
                <p>Access denied. Admin privileges required.</p>
            </div>
        `;
        return;
    }
    
    try {
        const response = await fetch('/api/users');
        if (!response.ok) {
            throw new Error('Failed to load users');
        }
        const users = await response.json();
        
        const content = document.getElementById('contentArea');
        
        if (users.length === 0) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">👤</div>
                    <p>No users found</p>
                </div>
            `;
            return;
        }
        
        content.innerHTML = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Username</th>
                            <th>Role</th>
                            <th>Status</th>
                            <th>Last Login</th>
                            <th>Created</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${users.map(u => {
                            const isSelf = u.user_id === currentUser.user_id;
                            return `
                            <tr class="${!u.is_active ? 'row-inactive' : ''} ${isSelf ? 'row-self' : ''}">
                                <td>${u.user_id}</td>
                                <td>
                                    <strong>${u.username}</strong>
                                    ${isSelf ? '<span class="badge badge-accent" style="margin-left: 0.5rem;">YOU</span>' : ''}
                                </td>
                                <td>
                                    <span class="badge ${u.role === 'admin' ? 'badge-danger' : u.role === 'operator' ? 'badge-success' : 'badge-info'}">
                                        ${u.role}
                                    </span>
                                </td>
                                <td>
                                    <span class="badge ${u.is_active ? 'badge-success' : 'badge-danger'}">
                                        ${u.is_active ? 'Active' : 'Inactive'}
                                    </span>
                                </td>
                                <td>${u.last_login || 'Never'}</td>
                                <td>${u.created_at || '-'}</td>
                                <td class="table-actions">
                                    <button class="btn btn-secondary btn-sm btn-icon" onclick="showEditUserForm(${u.user_id})" title="Edit">✏️</button>
                                    ${!isSelf ? `
                                        <button class="btn btn-${u.is_active ? 'warning' : 'success'} btn-sm btn-icon" onclick="toggleUserActive(${u.user_id}, ${!u.is_active})" title="${u.is_active ? 'Deactivate' : 'Activate'}">
                                            ${u.is_active ? '⏸️' : '▶️'}
                                        </button>
                                        <button class="btn btn-danger btn-sm btn-icon" onclick="deleteUser(${u.user_id})" title="Delete">🗑️</button>
                                    ` : '<span class="text-muted">—</span>'}
                                </td>
                            </tr>
                        `}).join('')}
                    </tbody>
                </table>
            </div>
            
            <div class="card" style="margin-top: 1.5rem;">
                <div class="card-title">📋 Role Permissions</div>
                <div class="role-permissions">
                    <div class="role-item">
                        <span class="badge badge-danger">admin</span>
                        <span>Full access: Read + Write + User Management</span>
                    </div>
                    <div class="role-item">
                        <span class="badge badge-success">operator</span>
                        <span>Read + Write (can modify Required/Net Required values)</span>
                    </div>
                    <div class="role-item">
                        <span class="badge badge-info">viewer</span>
                        <span>Read only (cannot modify values)</span>
                    </div>
                </div>
            </div>
        `;
    } catch (error) {
        showToast('Failed to load users', 'error');
        console.error(error);
    }
}

function showCreateUserForm() {
    showModal('Create User', `
        <form id="userForm">
            <div class="form-group">
                <label for="username">Username *</label>
                <input type="text" id="username" class="form-control" required minlength="3" placeholder="Enter username">
            </div>
            <div class="form-group">
                <label for="password">Password *</label>
                <input type="password" id="password" class="form-control" required minlength="6" placeholder="Min 6 characters">
            </div>
            <div class="form-group">
                <label for="role">Role *</label>
                <select id="role" class="form-control" required>
                    <option value="viewer">Viewer (Read only)</option>
                    <option value="operator">Operator (Read + Write)</option>
                </select>
                <small class="form-hint">To transfer admin role, edit an existing user.</small>
            </div>
            <div class="form-actions">
                <button type="button" class="btn btn-secondary" onclick="closeModal()">Cancel</button>
                <button type="submit" class="btn btn-success">Create User</button>
            </div>
        </form>
    `);
    
    document.getElementById('userForm').onsubmit = async (e) => {
        e.preventDefault();
        await createUser();
    };
}

async function createUser() {
    const data = {
        username: document.getElementById('username').value.trim(),
        password: document.getElementById('password').value,
        role: document.getElementById('role').value
    };
    
    try {
        const response = await fetch('/api/users', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        
        const result = await response.json();
        
        if (response.ok) {
            showToast('User created successfully', 'success');
            closeModal();
            loadUsers();
        } else {
            showToast(result.error || 'Failed to create user', 'error');
        }
    } catch (error) {
        showToast('Failed to create user', 'error');
    }
}

async function showEditUserForm(userId) {
    try {
        const response = await fetch(`/api/users/${userId}`);
        if (!response.ok) throw new Error('Failed to load user');
        const user = await response.json();
        
        // Check if editing yourself
        const isEditingSelf = userId === currentUser.user_id;
        const selfIsAdmin = currentUser.role === 'admin';
        
        // If admin edits themselves, disable role change
        const roleDisabled = isEditingSelf && selfIsAdmin;
        const roleSelectHtml = roleDisabled 
            ? `<select id="role" class="form-control" disabled>
                   <option value="admin" selected>Admin (Full access)</option>
               </select>
               <div class="form-info">
                   ℹ️ To change your admin role, first transfer admin to another user by editing them.
               </div>`
            : `<select id="role" class="form-control" required onchange="showAdminWarning(this)">
                   <option value="viewer" ${user.role === 'viewer' ? 'selected' : ''}>Viewer (Read only)</option>
                   <option value="operator" ${user.role === 'operator' ? 'selected' : ''}>Operator (Read + Write)</option>
                   <option value="admin" ${user.role === 'admin' ? 'selected' : ''}>Admin (Full access)</option>
               </select>
               <div id="adminWarning" class="form-warning" style="display: none;">
                   ⚠️ Transferring admin role to this user will demote YOU to operator.
               </div>`;
        
        showModal('Edit User', `
            <form id="userForm">
                <div class="form-group">
                    <label for="username">Username *</label>
                    <input type="text" id="username" class="form-control" required minlength="3" value="${user.username}">
                </div>
                <div class="form-group">
                    <label for="password">New Password (leave empty to keep current)</label>
                    <input type="password" id="password" class="form-control" minlength="6" placeholder="Leave empty to keep current password">
                </div>
                <div class="form-group">
                    <label for="role">Role *</label>
                    ${roleSelectHtml}
                </div>
                <div class="form-actions">
                    <button type="button" class="btn btn-secondary" onclick="closeModal()">Cancel</button>
                    <button type="submit" class="btn btn-success">Save Changes</button>
                </div>
            </form>
        `);
        
        document.getElementById('userForm').onsubmit = async (e) => {
            e.preventDefault();
            await updateUser(userId);
        };
    } catch (error) {
        showToast('Failed to load user', 'error');
    }
}

async function updateUser(userId) {
    const data = {
        username: document.getElementById('username').value.trim()
    };
    
    // Only include role if not disabled (admin editing themselves has disabled role)
    const roleSelect = document.getElementById('role');
    if (roleSelect && !roleSelect.disabled) {
        data.role = roleSelect.value;
    }
    
    const password = document.getElementById('password').value;
    if (password) {
        data.password = password;
    }
    
    try {
        const response = await fetch(`/api/users/${userId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        
        const result = await response.json();
        
        if (response.ok) {
            // Check if admin role was transferred
            if (result.role_transferred) {
                showToast(result.message || 'Admin role transferred. Reloading...', 'success');
                closeModal();
                // Reload page to reflect new role
                setTimeout(() => {
                    window.location.reload();
                }, 1500);
            } else {
                showToast('User updated successfully', 'success');
                closeModal();
                loadUsers();
                
                // Reload current user if we just updated ourselves
                if (userId === currentUser.user_id) {
                    await loadCurrentUser();
                }
            }
        } else {
            showToast(result.error || 'Failed to update user', 'error');
        }
    } catch (error) {
        showToast('Failed to update user', 'error');
    }
}

async function toggleUserActive(userId, isActive) {
    const action = isActive ? 'activate' : 'deactivate';
    if (!confirm(`Are you sure you want to ${action} this user?`)) {
        return;
    }
    
    try {
        const response = await fetch(`/api/users/${userId}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ is_active: isActive })
        });
        
        const result = await response.json();
        
        if (response.ok) {
            showToast(`User ${action}d successfully`, 'success');
            loadUsers();
        } else {
            showToast(result.error || `Failed to ${action} user`, 'error');
        }
    } catch (error) {
        showToast(`Failed to ${action} user`, 'error');
    }
}

async function deleteUser(userId) {
    if (!confirm('Are you sure you want to delete this user? This action cannot be undone.')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/users/${userId}`, { method: 'DELETE' });
        
        const result = await response.json();
        
        if (response.ok) {
            showToast('User deleted successfully', 'success');
            loadUsers();
        } else {
            showToast(result.error || 'Failed to delete user', 'error');
        }
    } catch (error) {
        showToast('Failed to delete user', 'error');
    }
}

function showAdminWarning(selectElement) {
    const warning = document.getElementById('adminWarning');
    if (warning) {
        warning.style.display = selectElement.value === 'admin' ? 'block' : 'none';
    }
}

// =============================================================================
// DEVICES (Configuration)
// =============================================================================

let devicesStatusInterval = null;

async function loadDevices() {
    // Clear any previous status refresh interval
    if (devicesStatusInterval) {
        clearInterval(devicesStatusInterval);
        devicesStatusInterval = null;
    }

    try {
        const response = await fetch('/api/devices');
        devices = await response.json();
        
        const content = document.getElementById('contentArea');
        
        if (devices.length === 0) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📟</div>
                    <p>No devices configured</p>
                </div>
            `;
            return;
        }
        
        content.innerHTML = `
            <div class="table-container">
                <table>
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Name</th>
                            <th>Model</th>
                            <th>Interface Protocol</th>
                            <th>Address</th>
                            <th>Status <span class="live-indicator">Live</span></th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${devices.map(d => `
                            <tr>
                                <td>${d.module_id}</td>
                                <td><strong>${d.module_name}</strong></td>
                                <td>${d.model_name || '<em>N/A</em>'}</td>
                                <td><span class="badge badge-info">${d.protocol || '-'}</span></td>
                                <td>${d.address_on_channel}</td>
                                <td id="device-status-${d.module_id}">
                                    <span class="badge ${d.is_connected ? 'badge-success' : 'badge-danger'}">
                                        ${d.is_connected ? '🟢 Online' : '🔴 Offline'}
                                    </span>
                                </td>
                                <td class="actions">
                                    <button class="btn btn-sm btn-primary" onclick="editDevice(${d.module_id})" title="Edit">✏️</button>
                                    <button class="btn btn-sm btn-danger" onclick="deleteDevice(${d.module_id})" title="Delete">🗑️</button>
                                </td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;

        // Start live status refresh
        devicesStatusInterval = setInterval(refreshDeviceStatuses, 200);
    } catch (error) {
        showToast('Failed to load devices', 'error');
    }
}

async function refreshDeviceStatuses() {
    if (currentPage !== 'devices') {
        clearInterval(devicesStatusInterval);
        devicesStatusInterval = null;
        return;
    }
    try {
        const response = await fetch('/api/devices');
        if (!response.ok) return;
        const freshDevices = await response.json();
        for (const d of freshDevices) {
            const cell = document.getElementById(`device-status-${d.module_id}`);
            if (cell) {
                const newHtml = `<span class="badge ${d.is_connected ? 'badge-success' : 'badge-danger'}">${d.is_connected ? '🟢 Online' : '🔴 Offline'}</span>`;
                if (cell.innerHTML.trim() !== newHtml.trim()) {
                    cell.innerHTML = newHtml;
                }
            }
        }
    } catch (e) { /* silently ignore refresh errors */ }
}



async function deleteDevice(id) {
    if (!confirm('Are you sure you want to delete this device? This will also remove all related I/O configurations.')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/devices/${id}`, { method: 'DELETE' });
        const result = await response.json();
        
        if (response.ok) {
            showToast('Device deleted successfully', 'success');
            loadDevices();
        } else {
            showToast(result.error || 'Failed to delete device', 'error');
        }
    } catch (error) {
        showToast('Failed to delete device', 'error');
    }
}

// =============================================================================
// MODAL & TOAST
// =============================================================================

function showModal(title, content) {
    document.getElementById('modalTitle').textContent = title;
    document.getElementById('modalBody').innerHTML = content;
    document.getElementById('modal').classList.add('show');
    // Default to normal width
    setModalWide(false);
}

function setModalWide(isWide) {
    const content = document.querySelector('.modal-content');
    if (isWide) {
        content.style.maxWidth = '90%';
        content.style.width = '1200px';
    } else {
        content.style.maxWidth = '600px';
        content.style.width = '90%';
    }
}

function closeModal() {
    document.getElementById('modal').classList.remove('show');
}

function showToast(message, type = 'info') {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = 'toast show ' + type;
    setTimeout(() => {
        toast.classList.remove('show');
    }, 3000);
}

// Close modal on escape
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeModal();
});

// Close modal on background click
document.getElementById('modal').addEventListener('click', (e) => {
    if (e.target.id === 'modal') closeModal();
});



