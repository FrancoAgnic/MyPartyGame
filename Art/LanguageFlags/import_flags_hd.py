import unreal
import os
import json

folder = '/Game/UI/Flags'
results = []
for language in ['ES','EN','PT','DE','FR','IT']:
    name = 'Flag_' + language + '_512'
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', os.path.join(unreal.Paths.project_content_dir(),'UI','Flags',name+'.png'))
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', False)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(folder+'/'+name)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Texture import failed: '+name)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError('Save failed: '+name)
    results.append({'asset':texture.get_path_name(), 'width':texture.blueprint_get_size_x(), 'height':texture.blueprint_get_size_y()})
    unreal.log('FLAG_HD_IMPORTED '+name)
with open(os.path.join(unreal.Paths.project_dir(),'Art','LanguageFlags','import_hd_results.json'),'w') as output:
    json.dump(results,output,indent=2)
