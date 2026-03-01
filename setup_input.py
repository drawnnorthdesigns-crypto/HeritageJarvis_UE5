"""
Heritage Jarvis — Input Asset Setup (UE5.7 compatible)
Run from UE5 editor: Tools → Execute Python Script → setup_input.py
"""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = '/Game/Blueprints'

# -------------------------------------------------------
# Helpers
# -------------------------------------------------------

def make_key(name):
    k = unreal.Key()
    k.set_editor_property('key_name', name)
    return k

def make_action(name, value_type):
    full_path = f'{FOLDER}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log(f'  [skip] {name} already exists')
        return unreal.load_asset(full_path)
    factory = unreal.InputActionFactory()
    action = asset_tools.create_asset(name, FOLDER, unreal.InputAction, factory)
    action.set_editor_property('value_type', value_type)
    unreal.EditorAssetLibrary.save_asset(full_path)
    unreal.log(f'  [ok]   {name}')
    return action

def add_modifiers(mapping, *mods):
    try:
        mapping.set_editor_property('modifiers', list(mods))
    except Exception as e:
        unreal.log_warning(f'  modifier skipped: {e}')

def swizzle_yxz():
    m = unreal.InputModifierSwizzleAxis()
    try:
        m.set_editor_property('order', unreal.InputAxisSwizzle.YXZ)
    except Exception:
        pass
    return m

def negate():
    return unreal.InputModifierNegate()

# -------------------------------------------------------
# Create Input Actions
# -------------------------------------------------------
unreal.log('=== Heritage Jarvis Input Setup ===')
unreal.log('Creating Input Actions...')

D  = unreal.InputActionValueType.BOOLEAN
A2 = unreal.InputActionValueType.AXIS2D

ia_move     = make_action('IA_Move',        A2)
ia_look     = make_action('IA_Look',        A2)
ia_jump     = make_action('IA_Jump',        D)
ia_sprint   = make_action('IA_Sprint',      D)
ia_interact = make_action('IA_Interact',    D)
ia_menu     = make_action('IA_OpenMenu',    D)
ia_debug    = make_action('IA_DebugToggle', D)

# -------------------------------------------------------
# Create Input Mapping Context
# -------------------------------------------------------
unreal.log('Creating IMC_Tartaria...')

imc_path = f'{FOLDER}/IMC_Tartaria'
if unreal.EditorAssetLibrary.does_asset_exist(imc_path):
    imc = unreal.load_asset(imc_path)
    unreal.log('  [skip] IMC_Tartaria already exists')
else:
    imc = asset_tools.create_asset(
        'IMC_Tartaria', FOLDER, unreal.InputMappingContext,
        unreal.InputMappingContextFactory())
    unreal.log('  [ok]   IMC_Tartaria')

# -------------------------------------------------------
# Map keys
# Axis2D: X = right/left, Y = forward/back
# Key press gives (1,0) by default.
# W/S need SwizzleAxis(YXZ) to push value into Y.
# S and A need Negate for negative direction.
# -------------------------------------------------------
unreal.log('Mapping keys...')

# Movement — WASD
m_w = imc.map_key(ia_move, make_key('W'))
add_modifiers(m_w, swizzle_yxz())               # W → Y+1 (forward)

m_s = imc.map_key(ia_move, make_key('S'))
add_modifiers(m_s, swizzle_yxz(), negate())     # S → Y-1 (backward)

m_d = imc.map_key(ia_move, make_key('D'))
# D → X+1 (right), no modifier needed

m_a = imc.map_key(ia_move, make_key('A'))
add_modifiers(m_a, negate())                    # A → X-1 (left)

# Look — mouse axes
imc.map_key(ia_look, make_key('MouseX'))
imc.map_key(ia_look, make_key('MouseY'))

# Digital actions
imc.map_key(ia_jump,     make_key('SpaceBar'))
imc.map_key(ia_sprint,   make_key('LeftShift'))
imc.map_key(ia_interact, make_key('E'))
imc.map_key(ia_menu,     make_key('Escape'))
imc.map_key(ia_debug,    make_key('F3'))

imc.modify()
unreal.EditorAssetLibrary.save_asset(imc_path)

unreal.log('=== Complete! All input assets ready. ===')
