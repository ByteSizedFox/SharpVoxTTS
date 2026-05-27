import SharpVoxModule from './sharpvox.js';

const Module = await SharpVoxModule();
const instance = new Module.SharpVoxInterop();
instance.Initialize();
self.postMessage({ type: 'ready' });

self.onmessage = ({ data }) => {
    switch (data.type) {
        case 'init':
            instance.SetMode(data.klattsch);
            break;
        case 'Speak':
            instance.Speak(data.text);
            break;
        case 'StopBtn':
            instance.StopBtn();
            break;
        case 'SetMode':
            instance.SetMode(data.klattsch);
            break;
        case 'UpdateParam':
            instance.UpdateParam(data.name, String(data.value));
            break;
        case 'OnPresetChange':
            instance.OnPresetChange(data.val);
            break;
        case 'HandleImport':
            instance.HandleImport(data.json);
            break;
        case 'ExportPreset':
            instance.ExportPreset();
            break;
        case 'DownloadWav':
            instance.DownloadWav(data.text);
            break;
        case 'AuditionPhoneme':
            instance.AuditionPhoneme(data.code);
            break;
        case 'ExportVideo':
            instance.ExportVideo(data.text);
            break;
        case 'ConvertUst': {
            const result = instance.ConvertUst(data.text, data.language, data.offset, data.bank);
            self.postMessage({ type: 'convertUstResult', json: result });
            break;
        }
    }
};
