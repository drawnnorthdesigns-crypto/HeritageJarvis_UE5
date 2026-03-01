import unreal

# Find all factory classes
factories = sorted([x for x in dir(unreal) if 'actory' in x])
unreal.log("Factories: " + str(factories))

# Find anything input-related
input_classes = sorted([x for x in dir(unreal) if 'nput' in x and 'actory' in x])
unreal.log("Input factories: " + str(input_classes))

# Try new_object approach
try:
    pkg = unreal.find_package('/Game/Blueprints')
    unreal.log("Package: " + str(pkg))
except Exception as e:
    unreal.log("Package: " + str(e))
