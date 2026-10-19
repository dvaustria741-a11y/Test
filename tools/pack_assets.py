import json, struct, sys, os, glob
import numpy as np
from PIL import Image
import meshoptimizer as mo
import etcpak

SRC_GLTF = sys.argv[1] if len(sys.argv) > 1 else 'kit/glTF'
SRC_TEX = sys.argv[2] if len(sys.argv) > 2 else 'tex/Textures'
OUT = sys.argv[3] if len(sys.argv) > 3 else 'app/src/main/assets'

MATERIALS = [
    ('BarkNormal',   'Bark_NormalTree.png',      1024, 0, 0),
    ('BarkDead',     'Bark_DeadTree.png',        1024, 0, 0),
    ('BarkTwisted',  'Bark_TwistedTree.png',     1024, 0, 0),
    ('LeafNormal',   'Leaves_NormalTree.png',    1024, 1, 1),
    ('LeafTwisted',  'Leaves_TwistedTree.png',   1024, 1, 1),
    ('LeafPine',     'Leaf_Pine.png',            1024, 1, 1),
    ('Plant',        'Leaves.png',               1024, 1, 1),
    ('Flowers',      'Flowers.png',               512, 1, 0),
    ('Grass',        'Grass.png',                 256, 0, 1),
    ('Mushrooms',    'Mushrooms.png',             512, 0, 0),
    ('PathRocks',    'PathRocks_Diffuse.png',     512, 0, 0),
    ('Rocks',        'Rocks_Diffuse.png',        1024, 0, 0),
    ('RocksDesert',  'Rocks_Desert_Diffuse.png', 1024, 0, 0),
]
MAT_INDEX = {m[0]: i for i, m in enumerate(MATERIALS)}
TEX_TO_MAT = {
    'Bark_NormalTree.png': 'BarkNormal', 'Bark_DeadTree.png': 'BarkDead', 'Bark_TwistedTree.png': 'BarkTwisted',
    'Leaves_NormalTree_C.png': 'LeafNormal', 'Leaves_TwistedTree_C.png': 'LeafTwisted', 'Leaf_Pine_C.png': 'LeafPine',
    'Leaves.png': 'Plant', 'Flowers.png': 'Flowers', 'Grass.png': 'Grass', 'Mushrooms.png': 'Mushrooms',
    'PathRocks_Diffuse.png': 'PathRocks', 'Rocks_Diffuse.png': 'Rocks',
}
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


def lod_ratios(tris):
    if tris >= 3000:
        return [1.0, 0.30, 0.09]
    if tris >= 800:
        return [1.0, 0.40]
    return [1.0]


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


def build_models():
    models = []
    for g in sorted(glob.glob(os.path.join(SRC_GLTF, '*.gltf'))):
        name = os.path.splitext(os.path.basename(g))[0]
        if name.startswith(SKIP):
            continue
        j = json.load(open(g))
        bins = [open(os.path.join(SRC_GLTF, b['uri']), 'rb').read() for b in j['buffers']]
        subs = []
        total_tris = 0
        for p in j['meshes'][0]['primitives']:
            mat = j['materials'][p['material']]
            tex = os.path.basename(j['images'][j['textures'][mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]['uri'])
            mname = TEX_TO_MAT[tex]
            pos = read_acc(j, bins, p['attributes']['POSITION']).astype(np.float32)
            nrm = read_acc(j, bins, p['attributes']['NORMAL']).astype(np.float32)
            uv = read_acc(j, bins, p['attributes']['TEXCOORD_0']).astype(np.float32)
            if 'COLOR_0' in p['attributes']:
                col = read_acc(j, bins, p['attributes']['COLOR_0']).astype(np.float32)
                if col.max() > 1.5:
                    col = col / 255.0
            else:
                col = np.ones((len(pos), 4), dtype=np.float32)
            idx = read_acc(j, bins, p['indices']).reshape(-1).astype(np.uint32)
            subs.append((mname, pos, nrm, uv, col, idx))
            total_tris += len(idx) // 3
        lods = []
        for ratio in lod_ratios(total_tris):
            lod = []
            for (mname, pos, nrm, uv, col, idx) in subs:
                nidx = idx if ratio >= 1.0 else simplify_sub(pos, nrm, uv, idx, ratio)
                if len(nidx) < 3:
                    continue
                cp, cn, cu, cc, ci = compact(pos, nrm, uv, col, nidx)
                lod.append((MAT_INDEX[mname], cp, cn, cu, cc, ci))
            tri = sum(len(x[5]) for x in lod)
            if lods and tri > 0.85 * sum(len(x[5]) for x in lods[-1]):
                continue
            lods.append(lod)
        allpos = np.concatenate([s[1] for s in subs])
        models.append((name, allpos.min(0), allpos.max(0), lods))
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
    vblob = bytearray()
    iblob = bytearray()
    head = bytearray()
    head += struct.pack('<4sII', b'NPK2', len(models), len(MATERIALS))
    for m in MATERIALS:
        head += struct.pack('<32s32sIII', m[0].encode(), m[1].encode(), m[2], m[3], m[4])
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
    im = Image.open(path).convert('RGBA')
    if im.size != (size, size):
        pm = np.asarray(im).astype(np.float32) / 255.0
        pm[..., :3] *= pm[..., 3:4]
        tmp = Image.fromarray(np.uint8(np.clip(pm * 255, 0, 255)), 'RGBA')
        tmp = tmp.resize((size, size), Image.LANCZOS)
        pm = np.asarray(tmp).astype(np.float32) / 255.0
        a = pm[..., 3:4]
        pm[..., :3] = np.where(a > 1e-4, pm[..., :3] / np.maximum(a, 1e-4), 0)
        arr = np.clip(pm, 0, 1)
    else:
        arr = np.asarray(im).astype(np.float32) / 255.0
    ref_cov = (arr[..., 3] > 0.4).mean()
    mips = []
    level = arr
    w = size
    fmt = 0x9278 if has_alpha else 0x9274
    while True:
        lv = level.copy()
        if has_alpha and len(mips) > 0:
            lv[..., 3] = coverage_fix(lv[..., 3], ref_cov)
        u8 = np.ascontiguousarray(np.clip(np.round(lv * 255), 0, 255).astype(np.uint8))
        rgba = u8.tobytes()
        if has_alpha:
            data = etcpak.compress_etc2_rgba(rgba, w, w)
        else:
            data = etcpak.compress_etc2_rgb(rgba, w, w)
        mips.append((w, bytes(data)))
        if w <= 4:
            break
        nw = w // 2
        pm = level.copy()
        pm[..., :3] *= pm[..., 3:4]
        pm = pm.reshape(nw, 2, nw, 2, 4).mean(axis=(1, 3))
        a = pm[..., 3:4]
        pm[..., :3] = np.where(a > 1e-4, pm[..., :3] / np.maximum(a, 1e-4), 0)
        level = pm
        w = nw
    with open(out_path, 'wb') as f:
        f.write(struct.pack('<4sIIII', b'NTX1', size, size, len(mips), fmt))
        for (mw, d) in mips:
            f.write(struct.pack('<II', mw, len(d)))
            f.write(d)
    return os.path.getsize(out_path)


def main():
    os.makedirs(os.path.join(OUT, 'tex'), exist_ok=True)
    models = build_models()
    size = write_models(models, os.path.join(OUT, 'props.pak'))
    print('props.pak', size // 1024, 'KB', len(models), 'models')
    tot = 0
    for (mname, fname, sz, alpha, tint) in MATERIALS:
        src = os.path.join(SRC_TEX, fname)
        s = build_texture(src, sz, bool(alpha), os.path.join(OUT, 'tex', mname + '.ntx'))
        tot += s
        print('tex', mname, sz, s // 1024, 'KB')
    print('textures total', tot // 1024, 'KB')
    names = [m[0] for m in models]
    with open(os.path.join(OUT, 'models.txt'), 'w') as f:
        f.write('\n'.join(names))
    for (name, mn, mx, lods) in models:
        print(name, [sum(len(s[5]) // 3 for s in lod) for lod in lods])


if __name__ == '__main__':
    main()
