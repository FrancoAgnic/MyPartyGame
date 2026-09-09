import unreal
import os
import json

results=[]
for name in ['T_Net_HighLatency_24','T_Net_Disconnected_24','T_Net_PacketLoss_24']:
    task=unreal.AssetImportTask()
    task.filename=os.path.join(unreal.Paths.project_content_dir(),'UI','NetworkStatus',name+'.png')
    task.destination_path='/Game/UI/NetworkStatus'
    task.destination_name=name
    task.automated=True
    task.replace_existing=False
    task.save=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.load_asset('/Game/UI/NetworkStatus/'+name)
    if not isinstance(texture,unreal.Texture2D): raise RuntimeError(name)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb',True)
    texture.set_editor_property('filter',unreal.TextureFilter.TF_BILINEAR)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture): raise RuntimeError('Save failed')
    w,h=texture.blueprint_get_size_x(),texture.blueprint_get_size_y()
    assert (w,h)==(24,24)
    results.append({'asset':texture.get_path_name(),'width':w,'height':h})
with open(os.path.join(unreal.Paths.project_dir(),'Art','NetworkStatus','import_results.json'),'w') as f:
    json.dump(results,f,indent=2)
