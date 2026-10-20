import argparse, json, struct, os, glob
import numpy as np
from PIL import Image
import meshoptimizer as mo
import etcpak

ap = argparse.ArgumentParser()
ap.add_argument('--nature-gltf', default='kit/glTF')
ap.add_argument('--nature-tex', default='tex/Textures')
ap.add_argument('--props-gltf', default='props/Exports/glTF')
ap.add_argument('--props-tex', default='props/Textures')
ap.add_argument('--village-gltf', default='vill/Medieval Village MegaKit[Standard]/glTF')
ap.add_argument('--village-tex', default='vill/Medieval Village MegaKit[Standard]/Textures')
ap.add_argument('--survival-obj', default='surv/Survival Pack - Sept 2020/OBJ')
ap.add_argument('--out', default='app/src/main/assets')
A = ap.parse_args()

TEXDIR = {'nature': A.nature_tex, 'props': A.props_tex, 'village': A.village_tex}


def M(name, src, file, size, alpha=0, boost=0.0, sway=0.0, ao=0.55, soft=0.0, dbl=0, vcol=0):
    return dict(name=name, src=src, file=file, size=size, alpha=alpha, boost=boost, sway=sway, ao=ao, soft=soft, dbl=dbl, vcol=vcol)


MATS = [
    M('BarkNormal', 'nature', 'Bark_NormalTree.png', 1024, sway=0.0012),
    M('BarkDead', 'nature', 'Bark_DeadTree.png', 1024, sway=0.0012),
    M('BarkTwisted', 'nature', 'Bark_TwistedTree.png', 1024, sway=0.0012),
    M('LeafNormal', 'nature', 'Leaves_NormalTree.png', 1024, 1, 1.0, 0.0012, 0.85, 0.55, 1),
    M('LeafTwisted', 'nature', 'Leaves_TwistedTree.png', 1024, 1, 1.0, 0.0012, 0.85, 0.55, 1),
    M('LeafPine', 'nature', 'Leaf_Pine.png', 1024, 1, 1.0, 0.0012, 0.85, 0.55, 1),
    M('Plant', 'nature', 'Leaves.png', 1024, 1, 2.0, 0.05, 0.80, 0.60, 1),
    M('Flowers', 'nature', 'Flowers.png', 512, 1, 0.0, 0.08, 0.85, 0.50, 1),
    M('Grass', 'nature', 'Grass.png', 256, 0, 1.15, 0.10, 0.78, 0.70, 1),
    M('Mushrooms', 'nature', 'Mushrooms.png', 512),
    M('PathRocks', 'nature', 'PathRocks_Diffuse.png', 512),
    M('Rocks', 'nature', 'Rocks_Diffuse.png', 1024),
    M('RocksDesert', 'nature', 'Rocks_Desert_Diffuse.png', 1024),
    M('WoodTrim', 'village', 'T_WoodTrim_BaseColor.png', 1024, ao=0.8),
    M('Plaster', 'village', 'T_Plaster_BaseColor.png', 1024, ao=0.8),
    M('UnevenBrick', 'village', 'T_UnevenBrick_BaseColor.png', 512, ao=0.8),
    M('RockTrim', 'village', 'T_RockTrim_BaseColor.png', 512, ao=0.8),
    M('Brick', 'village', 'T_Brick_BaseColor.png', 512, ao=0.8),
    M('TrimFurniture', 'props', 'T_Trim_Furniture_BaseColor.png', 1024, ao=0.8),
    M('TrimMetal', 'props', 'T_Trim_Metal_BaseColor.png', 512, ao=0.8),
    M('TrimProps', 'props', 'T_Trim_Props_BaseColor.png', 1024, ao=0.8),
    M('TrimCloth', 'props', 'T_Trim_Cloth_BaseColor.png', 512, ao=0.8),
    M('Flat', None, None, 8, ao=1.0, vcol=1),
]
MAT_INDEX = {m['name']: i for i, m in enumerate(MATS)}

TEX_TO_MAT = {
    'Bark_NormalTree.png': 'BarkNormal', 'Bark_DeadTree.png': 'BarkDead', 'Bark_TwistedTree.png': 'BarkTwisted',
    'Leaves_NormalTree_C.png': 'LeafNormal', 'Leaves_TwistedTree_C.png': 'LeafTwisted', 'Leaf_Pine_C.png': 'LeafPine',
    'Leaves.png': 'Plant', 'Flowers.png': 'Flowers', 'Grass.png': 'Grass', 'Mushrooms.png': 'Mushrooms',
    'PathRocks_Diffuse.png': 'PathRocks', 'Rocks_Diffuse.png': 'Rocks',
    'T_WoodTrim_BaseColor.png': 'WoodTrim', 'T_Plaster_BaseColor.png': 'Plaster', 'T_UnevenBrick_BaseColor.png': 'UnevenBrick',
    'T_RockTrim_BaseColor.png': 'RockTrim', 'T_Brick_BaseColor.png': 'Brick',
    'T_Trim_Furniture_BaseColor.png': 'TrimFurniture', 'T_Trim_Metal_BaseColor.png': 'TrimMetal',
    'T_Trim_Props_BaseColor.png': 'TrimProps', 'T_Trim_Cloth_BaseColor.png': 'TrimCloth',
}

PROPS = ['Axe_Bronze', 'Pickaxe_Bronze', 'Sword_Bronze', 'Torch_Metal', 'Workbench', 'Crate_Wooden', 'Chest_Wood', 'Barrel',
         'Bed_Twin1', 'Stool', 'Table_Large', 'Bench', 'Chair_1', 'Cauldron', 'Carrot', 'Bag', 'Bucket_Wooden_1', 'Rope_1']
VILLAGE = ['Floor_WoodDark', 'Floor_WoodLight', 'Wall_Plaster_Straight', 'Wall_Plaster_Door_Flat', 'Wall_Plaster_Window_Wide_Flat',
           'Wall_UnevenBrick_Straight', 'Wall_UnevenBrick_Door_Flat', 'Wall_UnevenBrick_Window_Wide_Flat',
           'Prop_WoodenFence_Single', 'Roof_Wooden_2x1']
SURVIVAL = [('Bonfire', 0.5), ('Bonfire_Fire', 0.5), ('WoodLog', 0.35), ('WoodenTorch', 0.3), ('WoodenTorch_Fire', 0.3),
            ('Tent', 0.14), ('Bandages', 1.5), ('WaterBottle_1', 0.8), ('Can_Closed', 1.2), ('FirstAidKit', 0.6), ('Backpack', 0.3)]
SKIP = ('Petal_',)


def read_acc(j, bins, i):
    a = j['accessors'][i]
    bv = j['bufferViews'][a['bufferView']]
    data = bins[bv['buffer']]
    ct = {5126: np.float32, 5123: np.uint16, 5121: np.uint8, 5125: np.uint32}[a['componentType']]
    nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    isz = np.dtype(ct).itemsize
    off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride') or nc * isz
    cnt = a['count']
    raw = np.frombuffer(data, dtype=np.uint8, offset=off, count=(cnt - 1) * stride + nc * isz)
    out = np.lib.stride_tricks.as_strided(raw, shape=(cnt, nc * isz), strides=(stride, 1)).copy().view(ct).reshape(cnt, nc)
    if a.get('normalized'):
        out = out.astype(np.float32) / float(np.iinfo(ct).max)
    return out


def load_gltf(path, scale=1.0):
    gdir = os.path.dirname(path)
    j = json.load(open(path))
    bins = [open(os.path.join(gdir, b['uri']), 'rb').read() for b in j['buffers']]
    subs = []
    for p in j['meshes'][0]['primitives']:
        mat = j['materials'][p['material']]
        tex = os.path.basename(j['images'][j['textures'][mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]['uri'])
        mname = TEX_TO_MAT[tex]
        pos = read_acc(j, bins, p['attributes']['POSITION']).astype(np.float32) * scale
        nrm = read_acc(j, bins, p['attributes']['NORMAL']).astype(np.float32)
        uv = read_acc(j, bins, p['attributes']['TEXCOORD_0']).astype(np.float32)
        if 'COLOR_0' in p['attributes']:
            col = read_acc(j, bins, p['attributes']['COLOR_0']).astype(np.float32)
            if col.max() > 1.5:
                col = col / 255.0
            if col.shape[1] == 3:
                col = np.concatenate([col, np.ones((len(col), 1), np.float32)], axis=1)
        else:
            col = np.ones((len(pos), 4), dtype=np.float32)
        idx = read_acc(j, bins, p['indices']).reshape(-1).astype(np.uint32)
        subs.append((mname, pos, nrm, uv, col, idx))
    return subs


def srgb(c):
    return np.clip(np.power(np.clip(c, 0, 1), 1.0 / 2.2), 0, 1)


def load_obj(path, scale):
    mtl = {}
    cur = None
    mpath = None
    for l in open(path):
        if l.startswith('mtllib'):
            mpath = os.path.join(os.path.dirname(path), l.split()[1])
    for l in open(mpath):
        t = l.split()
        if not t:
            continue
        if t[0] == 'newmtl':
            cur = t[1]
        elif t[0] == 'Kd' and cur:
            mtl[cur] = [float(x) for x in t[1:4]]
    V, N = [], []
    verts, key_map, idx = [], {}, []
    curm = None
    for l in open(path):
        t = l.split()
        if not t:
            continue
        if t[0] == 'v':
            V.append([float(x) for x in t[1:4]])
        elif t[0] == 'vn':
            N.append([float(x) for x in t[1:4]])
        elif t[0] == 'usemtl':
            curm = t[1]
        elif t[0] == 'f':
            fv = []
            for tok in t[1:]:
                p = tok.split('/')
                vi = int(p[0]) - 1
                ni = int(p[2]) - 1 if len(p) > 2 and p[2] else -1
                k = (vi, ni, curm)
                if k not in key_map:
                    key_map[k] = len(verts)
                    verts.append(k)
                fv.append(key_map[k])
            for i in range(1, len(fv) - 1):
                idx += [fv[0], fv[i], fv[i + 1]]
    pos = np.array([V[k[0]] for k in verts], np.float32) * scale
    nrm = np.array([N[k[1]] if k[1] >= 0 else [0, 1, 0] for k in verts], np.float32)
    col = np.ones((len(verts), 4), np.float32)
    for i, k in enumerate(verts):
        kd = np.array(mtl.get(k[2], [0.5, 0.5, 0.5]), np.float32)
        if k[2] == 'Fire':
            col[i] = [1.0, 0.62, 0.14, 0.0]
        else:
            col[i, :3] = srgb(kd)
    uv = np.zeros((len(verts), 2), np.float32)
    return [('Flat', pos, nrm, uv, col, np.array(idx, np.uint32))]


def simplify_sub(pos, nrm, uv, idx, ratio):
    target = max(int(len(idx) * ratio) // 3 * 3, min(len(idx), 36))
    attrs = np.ascontiguousarray(np.concatenate([nrm, uv], axis=1).astype(np.float32))
    weights = np.array([0.4, 0.4, 0.4, 2.0, 2.0], dtype=np.float32)
    src_idx = np.ascontiguousarray(idx.astype(np.uint32))
    dst = np.zeros(len(src_idx), dtype=np.uint32)
    pos32 = np.ascontiguousarray(pos.astype(np.float32))
    try:
        lock = np.zeros(len(pos32), dtype=np.uint8)
        n = mo.simplify_with_attributes(dst, src_idx, pos32, attrs, weights, vertex_lock=lock,
                                        target_index_count=target, target_error=1.0, options=mo.SIMPLIFY_PRUNE)
    except Exception:
        n = mo.simplify(dst, src_idx, pos32, target_index_count=target, target_error=1.0)
    if int(n) > target * 1.25 and ratio < 0.2:
        dst2 = np.zeros(len(src_idx), dtype=np.uint32)
        n2 = mo.simplify_sloppy(dst2, src_idx, pos32, target_index_count=target, target_error=1.0)
        if int(n2) >= 36:
            return dst2[:int(n2)].copy()
    return dst[:int(n)].copy()


def compact(pos, nrm, uv, col, idx):
    used = np.unique(idx)
    remap = np.full(len(pos), -1, dtype=np.int64)
    remap[used] = np.arange(len(used))
    return pos[used], nrm[used], uv[used], col[used], remap[idx].astype(np.uint16)


def lod_ratios(tris):
    if tris >= 3000:
        return [1.0, 0.30, 0.09]
    if tris >= 800:
        return [1.0, 0.40]
    return [1.0]


def make_model(name, subs, with_lod):
    tris = sum(len(s[5]) // 3 for s in subs)
    lods = []
    for ratio in (lod_ratios(tris) if with_lod else [1.0]):
        lod = []
        for (mname, pos, nrm, uv, col, idx) in subs:
            nidx = idx if ratio >= 1.0 else simplify_sub(pos, nrm, uv, idx, ratio)
            if len(nidx) < 3:
                continue
            cp, cn, cu, cc, ci = compact(pos, nrm, uv, col, nidx)
            lod.append((MAT_INDEX[mname], cp, cn, cu, cc, ci))
        t = sum(len(x[5]) for x in lod)
        if lods and t > 0.85 * sum(len(x[5]) for x in lods[-1]):
            continue
        lods.append(lod)
    allpos = np.concatenate([s[1] for s in subs])
    return (name, allpos.min(0), allpos.max(0), lods)


def build_models():
    models = []
    for g in sorted(glob.glob(os.path.join(glob.escape(A.nature_gltf), '*.gltf'))):
        name = os.path.splitext(os.path.basename(g))[0]
        if name.startswith(SKIP):
            continue
        models.append(make_model(name, load_gltf(g), True))
    for n in PROPS:
        models.append(make_model(n, load_gltf(os.path.join(A.props_gltf, n + '.gltf')), False))
    for n in VILLAGE:
        models.append(make_model(n, load_gltf(os.path.join(A.village_gltf, n + '.gltf')), False))
    for n, sc in SURVIVAL:
        models.append(make_model(n, load_obj(os.path.join(A.survival_obj, n + '.obj'), sc), False))
    return models


def pack_vertices(pos, nrm, uv, col):
    n = len(pos)
    buf = np.zeros(n, dtype=[('p', '<f4', 3), ('n', 'i1', 4), ('uv', '<f2', 2), ('c', 'u1', 4)])
    buf['p'] = pos
    nn = nrm / np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-6)
    nb = np.zeros((n, 4), dtype=np.int8)
    nb[:, :3] = np.clip(np.round(nn * 127), -127, 127).astype(np.int8)
    buf['n'] = nb
    buf['uv'] = uv.astype(np.float16)
    buf['c'] = np.clip(np.round(col * 255), 0, 255).astype(np.uint8)
    return buf.tobytes()


def write_models(models, path):
    vblob, iblob, head = bytearray(), bytearray(), bytearray()
    head += struct.pack('<4sII', b'NPK3', len(models), len(MATS))
    for m in MATS:
        head += struct.pack('<32s32sIIII4f', m['name'].encode(), (m['file'] or '').encode(), m['size'], m['alpha'], m['vcol'], m['dbl'],
                            m['boost'], m['sway'], m['ao'], m['soft'])
    for (name, mn, mx, lods) in models:
        head += struct.pack('<32s', name.encode())
        head += struct.pack('<6f', *[float(x) for x in mn], *[float(x) for x in mx])
        head += struct.pack('<I', len(lods))
        for lod in lods:
            head += struct.pack('<I', len(lod))
            for (mat, pos, nrm, uv, col, idx) in lod:
                voff = len(vblob)
                vblob += pack_vertices(pos, nrm, uv, col)
                ioff = len(iblob)
                iblob += idx.astype('<u2').tobytes()
                if len(iblob) % 4:
                    iblob += b'\0' * (4 - len(iblob) % 4)
                head += struct.pack('<IIIII', mat, voff, len(pos), ioff, len(idx))
    head += struct.pack('<II', len(vblob), len(iblob))
    with open(path, 'wb') as f:
        f.write(head)
        f.write(vblob)
        f.write(iblob)
    return len(head) + len(vblob) + len(iblob)


def coverage_fix(a, ref_cov, cutoff=0.4):
    lo, hi = 0.5, 4.0
    for _ in range(14):
        mid = (lo + hi) / 2
        cov = (np.clip(a * mid, 0, 1) > cutoff).mean()
        if cov < ref_cov:
            lo = mid
        else:
            hi = mid
    return np.clip(a * (lo + hi) / 2, 0, 1)


def build_texture(path, size, has_alpha, out_path):
    if path is None:
        arr = np.ones((size, size, 4), np.float32)
    else:
        im = Image.open(path).convert('RGBA')
        if im.size != (size, size):
            pm = np.asarray(im).astype(np.float32) / 255.0
            pm[..., :3] *= pm[..., 3:4]
            tmp = Image.fromarray(np.uint8(np.clip(pm * 255, 0, 255)), 'RGBA').resize((size, size), Image.LANCZOS)
            pm = np.asarray(tmp).astype(np.float32) / 255.0
            a = pm[..., 3:4]
            pm[..., :3] = np.where(a > 1e-4, pm[..., :3] / np.maximum(a, 1e-4), 0)
            arr = np.clip(pm, 0, 1)
        else:
            arr = np.asarray(im).astype(np.float32) / 255.0
    ref_cov = (arr[..., 3] > 0.4).mean()
    mips, level, w = [], arr, size
    fmt = 0x9278 if has_alpha else 0x9274
    while True:
        lv = level.copy()
        if has_alpha and len(mips) > 0:
            lv[..., 3] = coverage_fix(lv[..., 3], ref_cov)
        u8 = np.ascontiguousarray(np.clip(np.round(lv * 255), 0, 255).astype(np.uint8))
        rgba = u8.tobytes()
        data = etcpak.compress_etc2_rgba(rgba, w, w) if has_alpha else etcpak.compress_etc2_rgb(rgba, w, w)
        mips.append((w, bytes(data)))
        if w <= 4:
            break
        nw = w // 2
        pm = level.copy()
        pm[..., :3] *= pm[..., 3:4]
        pm = pm.reshape(nw, 2, nw, 2, 4).mean(axis=(1, 3))
        a = pm[..., 3:4]
        pm[..., :3] = np.where(a > 1e-4, pm[..., :3] / np.maximum(a, 1e-4), 0)
        level, w = pm, nw
    with open(out_path, 'wb') as f:
        f.write(struct.pack('<4sIIII', b'NTX1', size, size, len(mips), fmt))
        for (mw, d) in mips:
            f.write(struct.pack('<II', mw, len(d)))
            f.write(d)
    return os.path.getsize(out_path)


def main():
    os.makedirs(os.path.join(A.out, 'tex'), exist_ok=True)
    models = build_models()
    size = write_models(models, os.path.join(A.out, 'props.pak'))
    print('props.pak', size // 1024, 'KB', len(models), 'models')
    tot = 0
    for m in MATS:
        src = None if m['file'] is None else os.path.join(TEXDIR[m['src']], m['file'])
        s = build_texture(src, m['size'], bool(m['alpha']), os.path.join(A.out, 'tex', m['name'] + '.ntx'))
        tot += s
        print('tex', m['name'], m['size'], s // 1024, 'KB')
    print('textures total', tot // 1024, 'KB')
    with open(os.path.join(A.out, 'models.txt'), 'w') as f:
        f.write('\n'.join(m[0] for m in models))
    for (name, mn, mx, lods) in models:
        if name in PROPS or name in VILLAGE or name in [x[0] for x in SURVIVAL]:
            print('M', name, [sum(len(s[5]) // 3 for s in lod) for lod in lods], 'min', np.round(mn, 2).tolist(), 'max', np.round(mx, 2).tolist())


if __name__ == '__main__':
    main()
