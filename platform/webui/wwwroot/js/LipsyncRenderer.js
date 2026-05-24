import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';


const VRC_SHAPES = [
    'vrc.v_aa', 'vrc.v_ch', 'vrc.v_dd', 'vrc.v_e',  'vrc.v_ff',
    'vrc.v_ih', 'vrc.v_kk', 'vrc.v_nn', 'vrc.v_oh', 'vrc.v_ou',
    'vrc.v_pp', 'vrc.v_rr', 'vrc.v_sil','vrc.v_ss', 'vrc.v_th',
];

const SIZE = 512;

const threeRenderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
threeRenderer.setSize(SIZE, SIZE);

const scene = new THREE.Scene();
const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.001, 100);

scene.add(new THREE.AmbientLight(0xffffff, 0.7));
const keyLight = new THREE.DirectionalLight(0xfff5e8, 1.3);
keyLight.position.set(1, 1.5, 2);
scene.add(keyLight);
const fillLight = new THREE.DirectionalLight(0x9ab0e0, 0.4);
fillLight.position.set(-2, 0, 1);
scene.add(fillLight);

let morphTargets = {};
let weights = {};
let loaded = false;
let prevActive = null;
let onsetRemaining = 0;
let holdTime = 0;
let wobblePhase = 0;
let currentModelUrl = null;
let currentModelObj = null;
let loadGen = 0;
const ONSET_DUR = 0.1;    // 100ms soft-attack ramp when coming out of silence
const WOBBLE_RAMP = 0.35; // seconds to reach full wobble amplitude
const WOBBLE_MAX  = 0.06; // max wobble amplitude (clamped)
const WOBBLE_F1   = 5.3;  // Hz — primary wobble frequency
const WOBBLE_F2   = 8.7;  // Hz — secondary wobble frequency (inharmonic ratio → organic feel)

for (const name of VRC_SHAPES) weights[name] = 0;

function loadModel(url) {
    if (url === currentModelUrl && loaded) return Promise.resolve();
    const gen = ++loadGen;
    currentModelUrl = url;
    loaded = false;
    morphTargets = {};
    for (const name of VRC_SHAPES) weights[name] = 0;
    prevActive = null;
    onsetRemaining = 0;
    holdTime = 0;
    wobblePhase = 0;

    if (currentModelObj) {
        scene.remove(currentModelObj);
        currentModelObj = null;
    }

    return new Promise((resolve, reject) => {
    new GLTFLoader().load(url, gltf => {
        if (gen !== loadGen) { resolve(); return; }
        const model = gltf.scene;
        currentModelObj = model;
        scene.add(model);

        model.traverse(child => {
            if (!child.isMesh || !child.morphTargetDictionary) return;
            for (const name of VRC_SHAPES) {
                const idx = child.morphTargetDictionary[name];
                if (idx === undefined) continue;
                if (!morphTargets[name]) morphTargets[name] = [];
                morphTargets[name].push({ mesh: child, idx });
            }
        });

        const box = new THREE.Box3().setFromObject(model);
        const center = box.getCenter(new THREE.Vector3());
        const size = box.getSize(new THREE.Vector3());
        const d = Math.max(size.x, size.y, size.z);

        // Shift model so its bounding box center sits at world origin
        model.position.sub(center);

        const half = Math.max(size.x, size.y) / 2 * 1.4;
        camera.left   = -half;
        camera.right  =  half;
        camera.top    =  half;
        camera.bottom = -half;
        camera.near = d * 0.01;
        camera.far  = d * 20;
        camera.position.set(0, 0, d * 1.6);
        camera.lookAt(0, 0, 0);
        camera.updateProjectionMatrix();

        loaded = true;
        threeRenderer.render(scene, camera);
        resolve();
    }, undefined, reject);
    });
}

// Pre-load the default model
loadModel('FoxHead.glb');

function setShape(name, w) {
    for (const { mesh, idx } of morphTargets[name] ?? []) {
        mesh.morphTargetInfluences[idx] = w;
    }
}

function tick(v1, v2, progress, dt) {
    if (!loaded) return;
    const raw = (v2 && progress >= 0.5) ? v2 : v1;
    const active = raw || null;

    if (active !== prevActive) {
        if (prevActive === null && active !== null) onsetRemaining = ONSET_DUR;
        prevActive = active;
        holdTime = 0;
        wobblePhase = 0;
    }

    if (active !== null) holdTime += dt;
    wobblePhase += dt * Math.PI * 2 * WOBBLE_F1;
    const wobbleAmp = Math.min(holdTime / WOBBLE_RAMP, 1) * WOBBLE_MAX;
    // two inharmonic sines for an organic, non-mechanical feel
    const wobble = wobbleAmp * (0.6 * Math.sin(wobblePhase) + 0.4 * Math.sin(wobblePhase * (WOBBLE_F2 / WOBBLE_F1)));

    let speed = 20;
    if (onsetRemaining > 0) {
        const t = 1 - onsetRemaining / ONSET_DUR;  // 0→1 over ramp duration
        speed = 20 * (0.2 + 0.8 * t);              // ramps 4→20
        onsetRemaining = Math.max(0, onsetRemaining - dt);
    }

    const alpha = 1 - Math.exp(-dt * speed);
    for (const name of VRC_SHAPES) {
        const target = name === active ? 1 : 0;
        weights[name] += (target - weights[name]) * alpha;
        // wobble applied to output only — internal weight stays clean for smooth transitions
        const out = name === active ? Math.max(0, Math.min(1, weights[name] + wobble)) : weights[name];
        setShape(name, out);
    }
    threeRenderer.render(scene, camera);
}

window.lipsyncRenderer = {
    get ready() { return loaded; },
    get canvas() { return threeRenderer.domElement; },
    loadModel,
    tick,
};
