#include "renderer.h"
#include "platform.h"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

const char* kPropVS = R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aNrm;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aCol;
layout(location=4) in vec4 iPS;
layout(location=5) in float iYaw;
layout(location=6) in vec4 iTint;
uniform mat4 uVP;
uniform mat4 uLightVP;
uniform float uTime;
uniform float uSway;
uniform float uAOMin;
out vec2 vUV;
out vec3 vN;
out float vAO;
out vec3 vTint;
out vec3 vW;
out vec3 vSh;
void main(){
  float c = cos(iYaw);
  float s = sin(iYaw);
  vec3 p = aPos * iPS.w;
  vec3 n = aNrm.xyz;
  p = vec3(c*p.x + s*p.z, p.y, -s*p.x + c*p.z);
  n = vec3(c*n.x + s*n.z, n.y, -s*n.x + c*n.z);
  vec3 w = p + iPS.xyz;
  float ph = w.x*0.31 + w.z*0.23;
  float amp = uSway * p.y * p.y;
  w.x += sin(uTime*1.6 + ph) * amp;
  w.z += cos(uTime*1.2 + ph*1.4) * amp * 0.7;
  vUV = aUV;
  vN = n;
  vAO = mix(uAOMin, 1.0, aCol.r);
  vTint = iTint.rgb;
  vW = w;
  vSh = (uLightVP * vec4(w, 1.0)).xyz;
  gl_Position = uVP * vec4(w, 1.0);
}
)GLSL";

const char* kPropFS = R"GLSL(#version 300 es
precision mediump float;
in vec2 vUV;
in vec3 vN;
in float vAO;
in vec3 vTint;
in highp vec3 vW;
in highp vec3 vSh;
uniform sampler2D uTex;
uniform sampler2DShadow uShadow;
uniform int uMask;
uniform float uTintBoost;
uniform float uSoft;
uniform vec3 uSunDir;
uniform vec3 uSunCol;
uniform vec3 uAmbTop;
uniform vec3 uAmbBot;
uniform vec3 uFogCol;
uniform highp vec3 uCam;
uniform highp float uFogK;
uniform int uShadowOn;
uniform float uShadowTexel;
out vec4 oCol;
void main(){
  vec4 t = texture(uTex, vUV);
  if (uMask == 1 && t.a < 0.40) discard;
  vec3 base = t.rgb;
  if (uTintBoost > 0.0) {
    float l = dot(base, vec3(0.299, 0.587, 0.114));
    base = vTint * l * uTintBoost;
  }
  vec3 n = normalize(vN);
  if (!gl_FrontFacing) n = -n;
  n = normalize(mix(n, vec3(0.0, 1.0, 0.0), uSoft));
  vec3 V = normalize(uCam - vW);
  float ndl = dot(n, uSunDir);
  float wrap = ndl * 0.5 + 0.5;
  float toon = mix(wrap, smoothstep(0.22, 0.78, wrap), 0.45);
  float sh = 1.0;
  if (uShadowOn == 1 && ndl > -0.1) {
    float z = vSh.z - 0.0022;
    float k = uShadowTexel;
    sh = 0.25 * (texture(uShadow, vec3(vSh.xy + vec2(-k, -k), z)) + texture(uShadow, vec3(vSh.xy + vec2(k, -k), z)) +
                 texture(uShadow, vec3(vSh.xy + vec2(-k, k), z)) + texture(uShadow, vec3(vSh.xy + vec2(k, k), z)));
    vec2 e = abs(vSh.xy - 0.5) * 2.0;
    float edge = smoothstep(0.80, 1.0, max(e.x, e.y));
    sh = mix(sh, 1.0, edge);
    sh = mix(0.50, 1.0, sh);
  }
  vec3 amb = mix(uAmbBot, uAmbTop, n.y * 0.5 + 0.5);
  vec3 lit = (amb + uSunCol * toon * sh) * vAO;
  vec3 col = base * lit;
  float tr = pow(clamp(dot(V, -uSunDir), 0.0, 1.0), 3.0) * uSoft;
  col += base * uSunCol * tr * 0.55 * sh;
  col = 1.0 - exp(-col * 1.75);
  float lum = dot(col, vec3(0.299, 0.587, 0.114));
  col = clamp(mix(vec3(lum), col, 1.18), 0.0, 1.0);
  float d = length(vW - uCam);
  float f = 1.0 - exp(-d * d * uFogK * uFogK);
  col = mix(col, uFogCol, clamp(f, 0.0, 1.0));
  oCol = vec4(col, 1.0);
}
)GLSL";

const char* kShadowFS = R"GLSL(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform int uMask;
void main(){
  if (uMask == 1 && texture(uTex, vUV).a < 0.40) discard;
}
)GLSL";

const char* kTerrainVS = R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aNrm;
layout(location=2) in vec4 aCol;
uniform mat4 uVP;
uniform mat4 uLightVP;
out vec3 vN;
out vec4 vCol;
out vec3 vW;
out vec3 vSh;
void main(){
  vN = aNrm.xyz;
  vCol = aCol;
  vW = aPos;
  vSh = (uLightVP * vec4(aPos, 1.0)).xyz;
  gl_Position = uVP * vec4(aPos, 1.0);
}
)GLSL";

const char* kTerrainFS = R"GLSL(#version 300 es
precision highp float;
in vec3 vN;
in vec4 vCol;
in vec3 vW;
in vec3 vSh;
uniform sampler2DShadow uShadow;
uniform vec3 uSunDir;
uniform vec3 uSunCol;
uniform vec3 uAmbTop;
uniform vec3 uAmbBot;
uniform vec3 uFogCol;
uniform vec3 uCam;
uniform float uFogK;
uniform int uShadowOn;
uniform float uShadowTexel;
out vec4 oCol;
float h2(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vn(vec2 p){
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(h2(i), h2(i + vec2(1.0, 0.0)), f.x), mix(h2(i + vec2(0.0, 1.0)), h2(i + vec2(1.0, 1.0)), f.x), f.y);
}
void main(){
  float n = vn(vW.xz * 0.7) * 0.6 + vn(vW.xz * 2.9) * 0.4;
  vec3 base = vCol.rgb * (0.90 + 0.22 * n);
  float pm = smoothstep(0.32, 0.62, vCol.a + (n - 0.5) * 0.35);
  vec3 dirt = mix(vec3(0.62, 0.48, 0.26), vec3(0.78, 0.62, 0.36), n);
  base = mix(base, dirt, pm);
  vec3 nn = normalize(vN);
  float ndl = dot(nn, uSunDir);
  float wrap = ndl * 0.5 + 0.5;
  float toon = mix(wrap, smoothstep(0.22, 0.78, wrap), 0.45);
  float sh = 1.0;
  if (uShadowOn == 1) {
    float z = vSh.z - 0.0030;
    float k = uShadowTexel;
    sh = 0.25 * (texture(uShadow, vec3(vSh.xy + vec2(-k, -k), z)) + texture(uShadow, vec3(vSh.xy + vec2(k, -k), z)) +
                 texture(uShadow, vec3(vSh.xy + vec2(-k, k), z)) + texture(uShadow, vec3(vSh.xy + vec2(k, k), z)));
    vec2 e = abs(vSh.xy - 0.5) * 2.0;
    float edge = smoothstep(0.80, 1.0, max(e.x, e.y));
    sh = mix(sh, 1.0, edge);
    sh = mix(0.45, 1.0, sh);
  }
  vec3 amb = mix(uAmbBot, uAmbTop, nn.y * 0.5 + 0.5);
  vec3 col = base * (amb + uSunCol * toon * sh);
  col = 1.0 - exp(-col * 1.75);
  float lum = dot(col, vec3(0.299, 0.587, 0.114));
  col = clamp(mix(vec3(lum), col, 1.18), 0.0, 1.0);
  float d = length(vW - uCam);
  float f = 1.0 - exp(-d * d * uFogK * uFogK);
  col = mix(col, uFogCol, clamp(f, 0.0, 1.0));
  oCol = vec4(col, 1.0);
}
)GLSL";

const char* kSkyVS = R"GLSL(#version 300 es
precision highp float;
out vec2 vNdc;
void main(){
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vNdc = p * 2.0 - 1.0;
  gl_Position = vec4(vNdc, 1.0, 1.0);
}
)GLSL";

const char* kSkyFS = R"GLSL(#version 300 es
precision highp float;
in vec2 vNdc;
uniform vec3 uF;
uniform vec3 uR;
uniform vec3 uU;
uniform float uTanY;
uniform float uAspect;
uniform float uTime;
uniform vec3 uSunDir;
uniform int uClouds;
uniform vec3 uFogCol;
out vec4 oCol;
float h2(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vn(vec2 p){
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(h2(i), h2(i + vec2(1.0, 0.0)), f.x), mix(h2(i + vec2(0.0, 1.0)), h2(i + vec2(1.0, 1.0)), f.x), f.y);
}
void main(){
  vec3 dir = normalize(uF + uR * (vNdc.x * uTanY * uAspect) + uU * (vNdc.y * uTanY));
  float h = clamp(dir.y, 0.0, 1.0);
  vec3 zen = vec3(0.36, 0.62, 0.95);
  vec3 col = mix(uFogCol, zen, pow(h, 0.46));
  float sd = max(dot(dir, uSunDir), 0.0);
  col += vec3(1.0, 0.86, 0.58) * (pow(sd, 90.0) * 0.55 + pow(sd, 7.0) * 0.14);
  if (uClouds > 0 && dir.y > 0.01) {
    vec2 uv = dir.xz / (dir.y + 0.18) * 1.15 + vec2(uTime * 0.012, uTime * 0.004);
    float c = vn(uv) * 0.58 + vn(uv * 2.3) * 0.30;
    if (uClouds > 1) c += vn(uv * 5.1) * 0.12;
    float cl = smoothstep(0.50, 0.80, c) * smoothstep(0.0, 0.22, dir.y);
    vec3 cc = mix(vec3(0.86, 0.90, 0.97), vec3(1.0), smoothstep(0.55, 0.95, c));
    col = mix(col, cc, cl * 0.92);
  }
  if (dir.y < 0.0) col = mix(uFogCol, uFogCol * 0.95, clamp(-dir.y * 3.0, 0.0, 1.0));
  oCol = vec4(col, 1.0);
}
)GLSL";

const char* kUIVS = R"GLSL(#version 300 es
precision highp float;
uniform vec4 uRect;
uniform vec2 uScreen;
out vec2 vUV;
void main(){
  vec2 p = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
  vUV = p;
  vec2 px = uRect.xy + p * uRect.zw;
  gl_Position = vec4(px.x / uScreen.x * 2.0 - 1.0, 1.0 - px.y / uScreen.y * 2.0, 0.0, 1.0);
}
)GLSL";

const char* kUIFS = R"GLSL(#version 300 es
precision highp float;
in vec2 vUV;
uniform vec4 uColor;
uniform int uShape;
uniform float uParam;
uniform vec4 uUV;
uniform vec2 uSize;
uniform sampler2D uTex;
out vec4 oCol;
void main(){
  vec2 q = (vUV - 0.5) * 2.0;
  float a = 1.0;
  if (uShape == 0) {
    float d = length(q);
    float aa = fwidth(d) * 1.2;
    a = 1.0 - smoothstep(1.0 - aa, 1.0, d);
  } else if (uShape == 1) {
    float d = length(q);
    float aa = fwidth(d) * 1.2;
    float inner = 1.0 - uParam;
    a = (1.0 - smoothstep(1.0 - aa, 1.0, d)) * smoothstep(inner - aa, inner, d);
  } else if (uShape == 2) {
    float r = uParam * min(uSize.x, uSize.y) * 0.5;
    vec2 pp = q * 0.5 * uSize;
    vec2 b = 0.5 * uSize - vec2(r);
    vec2 dd = abs(pp) - b;
    float d = length(max(dd, 0.0)) + min(max(dd.x, dd.y), 0.0) - r;
    float aa = max(fwidth(d), 0.001) * 1.1;
    a = 1.0 - smoothstep(-aa, 0.0, d);
  } else if (uShape == 3) {
    vec4 t = texture(uTex, uUV.xy + vUV * uUV.zw);
    oCol = t * uColor;
    return;
  }
  oCol = vec4(uColor.rgb, uColor.a * a);
}
)GLSL";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        plat_log("shader compile error: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint link(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        plat_log("program link error: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

const V3 kSunDir = vnorm({0.42f, 0.78f, 0.46f});
const float kSunCol[3] = {0.78f, 0.68f, 0.50f};
const float kAmbTop[3] = {0.56f, 0.66f, 0.80f};
const float kAmbBot[3] = {0.40f, 0.38f, 0.30f};
const float kFogCol[3] = {0.84f, 0.92f, 0.98f};

}  // namespace

bool Renderer::buildPrograms() {
    props_.prog = link(kPropVS, kPropFS);
    propsShadow_.prog = link(kPropVS, kShadowFS);
    terrain_.prog = link(kTerrainVS, kTerrainFS);
    sky_.prog = link(kSkyVS, kSkyFS);
    ui_.prog = link(kUIVS, kUIFS);
    if (!props_.prog || !propsShadow_.prog || !terrain_.prog || !sky_.prog || !ui_.prog) return false;
    auto U = [](GLuint p, const char* n) { return glGetUniformLocation(p, n); };
    GLuint p = props_.prog;
    props_.uVP = U(p, "uVP"); props_.uLightVP = U(p, "uLightVP"); props_.uTime = U(p, "uTime"); props_.uSway = U(p, "uSway");
    props_.uTex = U(p, "uTex"); props_.uShadow = U(p, "uShadow"); props_.uMask = U(p, "uMask"); props_.uTintBoost = U(p, "uTintBoost");
    props_.uSunDir = U(p, "uSunDir"); props_.uSunCol = U(p, "uSunCol"); props_.uAmbTop = U(p, "uAmbTop"); props_.uAmbBot = U(p, "uAmbBot");
    props_.uAOMin = U(p, "uAOMin"); props_.uSoft = U(p, "uSoft"); props_.uFogCol = U(p, "uFogCol"); props_.uCam = U(p, "uCam"); props_.uFogK = U(p, "uFogK"); props_.uShadowOn = U(p, "uShadowOn");
    props_.uShadowTexel = U(p, "uShadowTexel");
    p = propsShadow_.prog;
    propsShadow_.uVP = U(p, "uVP"); propsShadow_.uLightVP = U(p, "uLightVP"); propsShadow_.uTime = U(p, "uTime");
    propsShadow_.uSway = U(p, "uSway"); propsShadow_.uTex = U(p, "uTex"); propsShadow_.uMask = U(p, "uMask");
    p = terrain_.prog;
    terrain_.uVP = U(p, "uVP"); terrain_.uLightVP = U(p, "uLightVP"); terrain_.uShadow = U(p, "uShadow");
    terrain_.uSunDir = U(p, "uSunDir"); terrain_.uSunCol = U(p, "uSunCol"); terrain_.uAmbTop = U(p, "uAmbTop");
    terrain_.uAmbBot = U(p, "uAmbBot"); terrain_.uFogCol = U(p, "uFogCol"); terrain_.uCam = U(p, "uCam");
    terrain_.uFogK = U(p, "uFogK"); terrain_.uShadowOn = U(p, "uShadowOn"); terrain_.uShadowTexel = U(p, "uShadowTexel");
    p = sky_.prog;
    sky_.uF = U(p, "uF"); sky_.uR = U(p, "uR"); sky_.uU = U(p, "uU"); sky_.uTanY = U(p, "uTanY"); sky_.uAspect = U(p, "uAspect");
    sky_.uTime = U(p, "uTime"); sky_.uSunDir = U(p, "uSunDir"); sky_.uClouds = U(p, "uClouds"); sky_.uFogCol = U(p, "uFogCol");
    p = ui_.prog;
    ui_.uRect = U(p, "uRect"); ui_.uScreen = U(p, "uScreen"); ui_.uColor = U(p, "uColor"); ui_.uShape = U(p, "uShape");
    ui_.uParam = U(p, "uParam"); ui_.uTex = U(p, "uTex"); ui_.uUV = U(p, "uUV"); ui_.uSize = U(p, "uSize");
    return true;
}

bool Renderer::uploadPak() {
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)pak_->vblobSize, pak_->blob.data() + pak_->vblobOff, GL_STATIC_DRAW);
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)pak_->iblobSize, pak_->blob.data() + pak_->iblobOff, GL_STATIC_DRAW);
    vaos_.assign(pak_->models.size(), {});
    for (size_t m = 0; m < pak_->models.size(); m++) {
        const ModelDef& md = pak_->models[m];
        vaos_[m].resize(md.lods.size());
        for (size_t l = 0; l < md.lods.size(); l++) {
            vaos_[m][l].resize(md.lods[l].size());
            for (size_t s = 0; s < md.lods[l].size(); s++) {
                const SubMesh& sm = md.lods[l][s];
                GLuint vao;
                glGenVertexArrays(1, &vao);
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                size_t b = sm.voff;
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (const void*)(b + 0));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, 24, (const void*)(b + 12));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_HALF_FLOAT, GL_FALSE, 24, (const void*)(b + 16));
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 24, (const void*)(b + 20));
                for (int a = 4; a <= 6; a++) {
                    glEnableVertexAttribArray((GLuint)a);
                    glVertexAttribDivisor((GLuint)a, 1);
                }
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
                vaos_[m][l][s] = vao;
            }
        }
    }
    glBindVertexArray(0);
    return true;
}

void Renderer::freeTextures() {
    for (GLuint t : tex_) if (t) glDeleteTextures(1, &t);
    tex_.clear();
}

void Renderer::loadTextures() {
    freeTextures();
    tex_.assign(pak_->mats.size(), 0);
    for (size_t i = 0; i < pak_->mats.size(); i++) {
        std::string path = std::string("tex/") + pak_->mats[i].name + ".ntx";
        std::vector<uint8_t> data;
        if (!plat_read_asset(path.c_str(), data) || data.size() < 20) { plat_log("missing texture %s", path.c_str()); continue; }
        uint32_t hdr[5];
        std::memcpy(hdr, data.data(), 20);
        uint32_t w0 = hdr[1], mips = hdr[3], fmt = hdr[4];
        struct MipRef { uint32_t w, size; size_t off; };
        std::vector<MipRef> refs;
        size_t p = 20;
        for (uint32_t m = 0; m < mips; m++) {
            if (p + 8 > data.size()) break;
            uint32_t mw, sz;
            std::memcpy(&mw, data.data() + p, 4);
            std::memcpy(&sz, data.data() + p + 4, 4);
            p += 8;
            if (p + sz > data.size()) break;
            refs.push_back({mw, sz, p});
            p += sz;
        }
        int skip = q_.texSkip;
        while (skip > 0 && (int)(w0 >> skip) < 64) skip--;
        if ((size_t)skip >= refs.size()) skip = 0;
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        int level = 0;
        for (size_t m = (size_t)skip; m < refs.size(); m++, level++) {
            glCompressedTexImage2D(GL_TEXTURE_2D, level, fmt, (GLsizei)refs[m].w, (GLsizei)refs[m].w, 0, (GLsizei)refs[m].size,
                                   data.data() + refs[m].off);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level - 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, q_.trilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        tex_[i] = t;
    }
}

void Renderer::freeTargets() {
    if (sceneFbo_) glDeleteFramebuffers(1, &sceneFbo_);
    if (sceneColor_) glDeleteRenderbuffers(1, &sceneColor_);
    if (sceneDepth_) glDeleteRenderbuffers(1, &sceneDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    sceneFbo_ = sceneColor_ = sceneDepth_ = shadowFbo_ = shadowTex_ = 0;
    sceneW_ = sceneH_ = 0;
    shadowRes_ = 0;
}

void Renderer::createTargets() {
    freeTargets();
    if (W_ <= 0 || H_ <= 0) return;
    if (q_.renderMax < 0.97f) {
        sceneW_ = std::max(64, (int)(W_ * q_.renderMax + 0.5f));
        sceneH_ = std::max(64, (int)(H_ * q_.renderMax + 0.5f));
        glGenFramebuffers(1, &sceneFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
        glGenRenderbuffers(1, &sceneColor_);
        glBindRenderbuffer(GL_RENDERBUFFER, sceneColor_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, sceneW_, sceneH_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColor_);
        glGenRenderbuffers(1, &sceneDepth_);
        glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth_);
        glRenderbufferStorage(GL_RENDERBUFFER, q_.tier == 0 ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24, sceneW_, sceneH_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            plat_log("scene fbo incomplete");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &sceneFbo_);
            glDeleteRenderbuffers(1, &sceneColor_);
            glDeleteRenderbuffers(1, &sceneDepth_);
            sceneFbo_ = sceneColor_ = sceneDepth_ = 0;
            sceneW_ = sceneH_ = 0;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    if (q_.shadows) {
        shadowRes_ = q_.shadowRes;
        glGenTextures(1, &shadowTex_);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowRes_, shadowRes_, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenFramebuffers(1, &shadowFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            plat_log("shadow fbo incomplete");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &shadowFbo_);
            glDeleteTextures(1, &shadowTex_);
            shadowFbo_ = shadowTex_ = 0;
            shadowRes_ = 0;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

bool Renderer::init(const Pak& pak, const Quality& q, int w, int h) {
    pak_ = &pak;
    q_ = q;
    W_ = w;
    H_ = h;
    if (!buildPrograms()) return false;
    uploadPak();
    glGenVertexArrays(1, &emptyVao_);
    glGenBuffers(2, instVbo_);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_[i]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(instCap_ * sizeof(GPUInst)), nullptr, GL_STREAM_DRAW);
    }
    size_t nm = pak.mats.size();
    matSway_.assign(nm, 0.f);
    matBoost_.assign(nm, 0.f);
    matAO_.assign(nm, 0.55f);
    matSoft_.assign(nm, 0.f);
    matMask_.assign(nm, 0);
    matDouble_.assign(nm, 0);
    for (size_t i = 0; i < nm; i++) {
        std::string n = pak.mats[i].name;
        matMask_[i] = (int)pak.mats[i].alpha;
        if (n.rfind("Bark", 0) == 0) { matSway_[i] = 0.0012f; }
        else if (n.rfind("Leaf", 0) == 0) { matSway_[i] = 0.0012f; matBoost_[i] = 1.0f; matDouble_[i] = 1; matAO_[i] = 0.85f; matSoft_[i] = 0.55f; }
        else if (n == "Plant") { matSway_[i] = 0.05f; matBoost_[i] = 2.0f; matDouble_[i] = 1; matAO_[i] = 0.80f; matSoft_[i] = 0.6f; }
        else if (n == "Flowers") { matSway_[i] = 0.08f; matDouble_[i] = 1; matAO_[i] = 0.85f; matSoft_[i] = 0.5f; }
        else if (n == "Grass") { matSway_[i] = 0.10f; matBoost_[i] = 1.15f; matDouble_[i] = 1; matAO_[i] = 0.78f; matSoft_[i] = 0.7f; }
        if (n == "Rocks") rocksMat_ = (int)i;
        if (n == "RocksDesert") rocksDesertMat_ = (int)i;
    }
    loadTextures();
    buildFont();
    createTargets();
    lists_.init(pak.models.size() * 6);
    shadowLists_.init(pak.models.size() * 6);
    scratch_.reserve(instCap_);
    return true;
}

void Renderer::destroy() {
    if (fontTex_) glDeleteTextures(1, &fontTex_);
    fontTex_ = 0;
    freeTextures();
    freeTargets();
    for (auto& a : vaos_) for (auto& b : a) for (GLuint v : b) if (v) glDeleteVertexArrays(1, &v);
    vaos_.clear();
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    vbo_ = ibo_ = 0;
    if (instVbo_[0]) glDeleteBuffers(2, instVbo_);
    instVbo_[0] = instVbo_[1] = 0;
    if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
    emptyVao_ = 0;
    GLuint progs[] = {props_.prog, propsShadow_.prog, terrain_.prog, sky_.prog, ui_.prog};
    for (GLuint p : progs) if (p) glDeleteProgram(p);
    props_ = ProgProps(); propsShadow_ = ProgProps(); terrain_ = ProgTerrain(); sky_ = ProgSky(); ui_ = ProgUI();
}

void Renderer::resize(int w, int h) {
    if (w == W_ && h == H_) return;
    W_ = w;
    H_ = h;
    createTargets();
}

void Renderer::setQuality(const Quality& q) {
    bool texChange = q.texSkip != q_.texSkip || q.trilinear != q_.trilinear;
    bool targetChange = q.renderMax != q_.renderMax || q.shadows != q_.shadows || q.shadowRes != q_.shadowRes || q.tier != q_.tier;
    q_ = q;
    if (texChange && pak_) loadTextures();
    if (targetChange) createTargets();
}

GLuint Renderer::loadUITexture(const char* path, int* outW, int* outH) {
    std::vector<uint8_t> data;
    if (!plat_read_asset(path, data)) return 0;
    int w, h, n;
    unsigned char* px = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &n, 4);
    if (!px) return 0;
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    if (outW) *outW = w;
    if (outH) *outH = h;
    return t;
}

void Renderer::collect(const Camera& cam, const Frustum& fr, ChunkManager& cm) {
    lists_.clear();
    const float dens255 = q_.density * 255.f;
    const float catDist[5] = {q_.treeDist, q_.rockDist, q_.bushDist, q_.detailDist, q_.pebbleDist};
    const float maxChunkDist = q_.treeDist + 80.f;
    const size_t cap = instCap_ * 7 / 10;
    for (const auto& kv : cm.chunks()) {
        const Chunk& c = kv.second;
        float x0 = (float)(c.cx * CHUNK_SIZE), z0 = (float)(c.cz * CHUNK_SIZE);
        float cdx = std::max(std::max(x0 - cam.pos.x, 0.f), cam.pos.x - (x0 + CHUNK_SIZE));
        float cdz = std::max(std::max(z0 - cam.pos.z, 0.f), cam.pos.z - (z0 + CHUNK_SIZE));
        if (cdx * cdx + cdz * cdz > maxChunkDist * maxChunkDist) continue;
        if (!frustumAABB(fr, x0, c.minY, z0, x0 + CHUNK_SIZE, c.maxY + 34.f, z0 + CHUNK_SIZE)) continue;
        for (int pass = 0; pass < 2; pass++) {
            if (pass == 1 && c.detailState != 2) break;
            const std::vector<PropInst>& v = pass == 0 ? c.structs : c.detail;
            for (const PropInst& p : v) {
                const ModelDef& md = pak_->models[p.model];
                float dx = p.x - cam.pos.x, dz = p.z - cam.pos.z;
                float d2 = dx * dx + dz * dz;
                float maxD = catDist[md.cat];
                if (d2 > maxD * maxD) continue;
                bool small = md.cat >= CAT_DETAIL;
                if (small && (float)p.rnd > dens255) continue;
                float d = std::sqrt(d2);
                float s = p.s;
                if (small) {
                    float f = 1.f - smoothstepf(0.72f, 1.f, d / maxD);
                    if (f < 0.03f) continue;
                    s *= f;
                }
                if (!frustumSphere(fr, p.x, p.y + md.cy * s, p.z, md.radius * s)) continue;
                int nl = (int)md.lods.size();
                float k = md.cat == CAT_TREE ? 1.f : 0.6f;
                int lod = 0;
                if (nl == 2) lod = d < q_.lod1 * k ? 0 : 1;
                else if (nl >= 3) lod = d < q_.lod0 * k ? 0 : (d < q_.lod1 * k ? 1 : 2);
                if (lists_.total >= cap) return;
                size_t idx = ((size_t)p.model * 2 + (size_t)(p.var ? 1 : 0)) * 3 + (size_t)lod;
                lists_.add(idx, GPUInst{p.x, p.y, p.z, s, p.yaw, p.tint});
            }
        }
    }
}

void Renderer::collectShadow(const Camera& cam, ChunkManager& cm) {
    shadowLists_.clear();
    const float R = q_.shadowDist;
    const size_t cap = instCap_ * 25 / 100;
    for (const auto& kv : cm.chunks()) {
        const Chunk& c = kv.second;
        float x0 = (float)(c.cx * CHUNK_SIZE), z0 = (float)(c.cz * CHUNK_SIZE);
        float cdx = std::max(std::max(x0 - cam.pos.x, 0.f), cam.pos.x - (x0 + CHUNK_SIZE));
        float cdz = std::max(std::max(z0 - cam.pos.z, 0.f), cam.pos.z - (z0 + CHUNK_SIZE));
        if (cdx * cdx + cdz * cdz > (R + 20.f) * (R + 20.f)) continue;
        for (const PropInst& p : c.structs) {
            float dx = p.x - cam.pos.x, dz = p.z - cam.pos.z;
            float d2 = dx * dx + dz * dz;
            if (d2 > (R + 10.f) * (R + 10.f)) continue;
            const ModelDef& md = pak_->models[p.model];
            int nl = (int)md.lods.size();
            int lod = (std::sqrt(d2) < 28.f || nl < 2) ? 0 : 1;
            if (nl >= 3 && lod == 1 && std::sqrt(d2) > 55.f) lod = 2;
            if (lod >= nl) lod = nl - 1;
            if (shadowLists_.total >= cap) return;
            size_t idx = ((size_t)p.model * 2 + (size_t)(p.var ? 1 : 0)) * 3 + (size_t)lod;
            shadowLists_.add(idx, GPUInst{p.x, p.y, p.z, p.s, p.yaw, p.tint});
        }
    }
}

void Renderer::drawGroups(const InstanceLists& L, bool shadow) {
    const ProgProps& P = shadow ? propsShadow_ : props_;
    int lastMat = -1;
    for (int gi : L.active) {
        const std::vector<GPUInst>& vec = L.g[(size_t)gi];
        size_t model = (size_t)gi / 6;
        int var = (gi / 3) % 2;
        size_t lod = (size_t)gi % 3;
        const ModelDef& md = pak_->models[model];
        if (lod >= md.lods.size()) continue;
        size_t byteOff = L.off[(size_t)gi] * sizeof(GPUInst);
        for (size_t s = 0; s < md.lods[lod].size(); s++) {
            const SubMesh& sm = md.lods[lod][s];
            int mat = (int)sm.mat;
            if (var == 1 && mat == rocksMat_ && rocksDesertMat_ >= 0) mat = rocksDesertMat_;
            if (mat != lastMat) {
                lastMat = mat;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex_[(size_t)mat]);
                glUniform1i(P.uMask, matMask_[(size_t)mat]);
                glUniform1f(P.uSway, matSway_[(size_t)mat]);
                if (!shadow) {
                    glUniform1f(props_.uTintBoost, matBoost_[(size_t)mat]);
                    glUniform1f(props_.uAOMin, matAO_[(size_t)mat]);
                    glUniform1f(props_.uSoft, matSoft_[(size_t)mat]);
                }
                if (!shadow) {
                    if (matDouble_[(size_t)mat]) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
                }
            }
            glBindVertexArray(vaos_[model][lod][s]);
            glBindBuffer(GL_ARRAY_BUFFER, instVbo_[instRing_]);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GPUInst), (const void*)(byteOff + 0));
            glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(GPUInst), (const void*)(byteOff + 16));
            glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GPUInst), (const void*)(byteOff + 20));
            glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)sm.icount, GL_UNSIGNED_SHORT, (const void*)(uintptr_t)sm.ioff, (GLsizei)vec.size());
            lastDrawCalls_++;
        }
    }
}

void Renderer::render(const Camera& cam, ChunkManager& cm, float time, float renderScale) {
    time_ = time;
    camPos_ = cam.pos;
    lastDrawCalls_ = 0;
    fogK_ = 2.0f / q_.viewDist;
    float cy = std::cos(cam.pitch), sy = std::sin(cam.pitch);
    V3 fwd = {std::sin(cam.yaw) * cy, sy, -std::cos(cam.yaw) * cy};
    V3 right = vnorm(cross(fwd, V3{0, 1, 0}));
    V3 up = cross(right, fwd);
    float farD = q_.viewDist * 1.35f + 40.f;
    Mat4 proj = mat4Perspective(cam.fov, cam.aspect, 0.25f, farD);
    Mat4 view = mat4LookAt(cam.pos, cam.pos + fwd, V3{0, 1, 0});
    Mat4 vp = mat4Mul(proj, view);
    Frustum fr;
    frustumFromVP(vp, fr);

    collect(cam, fr, cm);
    shadowActive_ = q_.shadows && shadowFbo_ != 0;
    if (shadowActive_) collectShadow(cam, cm);

    scratch_.clear();
    auto flatten = [&](InstanceLists& L) {
        for (int gi : L.active) {
            L.off[(size_t)gi] = scratch_.size();
            const auto& v = L.g[(size_t)gi];
            scratch_.insert(scratch_.end(), v.begin(), v.end());
        }
    };
    flatten(lists_);
    size_t shadowBase = scratch_.size();
    (void)shadowBase;
    if (shadowActive_) flatten(shadowLists_);
    instRing_ ^= 1;
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_[instRing_]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(instCap_ * sizeof(GPUInst)), nullptr, GL_STREAM_DRAW);
    if (!scratch_.empty()) glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(std::min(scratch_.size(), instCap_) * sizeof(GPUInst)), scratch_.data());
    lastInstances_ = (int)lists_.total;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    Mat4 lightBias = {};
    if (shadowActive_) {
        float R = q_.shadowDist;
        V3 center = {cam.pos.x, cam.pos.y - 1.7f, cam.pos.z};
        V3 f = V3{-kSunDir.x, -kSunDir.y, -kSunDir.z};
        V3 sx = vnorm(cross(f, V3{0, 1, 0}));
        V3 uy = cross(sx, f);
        float texel = 2.f * R / (float)shadowRes_;
        float cs = dot(sx, center), cu = dot(uy, center);
        float ss = std::floor(cs / texel) * texel, su = std::floor(cu / texel) * texel;
        center = center + sx * (ss - cs) + uy * (su - cu);
        V3 eye = center + kSunDir * 140.f;
        Mat4 lv = mat4LookAt(eye, center, V3{0, 1, 0});
        Mat4 lp = mat4Ortho(-R, R, -R, R, 5.f, 300.f);
        lightRaw_ = mat4Mul(lp, lv);
        Mat4 B;
        std::memset(B.m, 0, sizeof(B.m));
        B.m[0] = 0.5f; B.m[5] = 0.5f; B.m[10] = 0.5f; B.m[12] = 0.5f; B.m[13] = 0.5f; B.m[14] = 0.5f; B.m[15] = 1.f;
        lightBias = mat4Mul(B, lightRaw_);

        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, shadowRes_, shadowRes_);
        glClear(GL_DEPTH_BUFFER_BIT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        glDisable(GL_CULL_FACE);
        glUseProgram(propsShadow_.prog);
        glUniformMatrix4fv(propsShadow_.uVP, 1, GL_FALSE, lightRaw_.m);
        glUniformMatrix4fv(propsShadow_.uLightVP, 1, GL_FALSE, lightBias.m);
        glUniform1f(propsShadow_.uTime, time_);
        glUniform1i(propsShadow_.uTex, 0);
        drawGroups(shadowLists_, true);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        GLenum att = GL_DEPTH_ATTACHMENT;
        (void)att;
    }

    bool scaled = sceneFbo_ != 0 && renderScale < 0.97f;
    int rw = W_, rh = H_;
    if (scaled) {
        rw = std::max(64, std::min(sceneW_, (int)(W_ * renderScale)));
        rh = std::max(64, std::min(sceneH_, (int)(H_ * renderScale)));
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    glViewport(0, 0, rw, rh);
    glClearColor(kFogCol[0], kFogCol[1], kFogCol[2], 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(sky_.prog);
    glUniform3f(sky_.uF, fwd.x, fwd.y, fwd.z);
    glUniform3f(sky_.uR, right.x, right.y, right.z);
    glUniform3f(sky_.uU, up.x, up.y, up.z);
    glUniform1f(sky_.uTanY, std::tan(cam.fov * 0.5f));
    glUniform1f(sky_.uAspect, cam.aspect);
    glUniform1f(sky_.uTime, time_);
    glUniform3f(sky_.uSunDir, kSunDir.x, kSunDir.y, kSunDir.z);
    glUniform1i(sky_.uClouds, q_.clouds);
    glUniform3f(sky_.uFogCol, kFogCol[0], kFogCol[1], kFogCol[2]);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);

    if (shadowActive_) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glActiveTexture(GL_TEXTURE0);
    }
    float texel = shadowRes_ > 0 ? 0.6f / (float)shadowRes_ : 0.f;

    glDisable(GL_CULL_FACE);
    glUseProgram(terrain_.prog);
    glUniformMatrix4fv(terrain_.uVP, 1, GL_FALSE, vp.m);
    glUniformMatrix4fv(terrain_.uLightVP, 1, GL_FALSE, lightBias.m);
    glUniform1i(terrain_.uShadow, 1);
    glUniform3f(terrain_.uSunDir, kSunDir.x, kSunDir.y, kSunDir.z);
    glUniform3f(terrain_.uSunCol, kSunCol[0], kSunCol[1], kSunCol[2]);
    glUniform3f(terrain_.uAmbTop, kAmbTop[0], kAmbTop[1], kAmbTop[2]);
    glUniform3f(terrain_.uAmbBot, kAmbBot[0], kAmbBot[1], kAmbBot[2]);
    glUniform3f(terrain_.uFogCol, kFogCol[0], kFogCol[1], kFogCol[2]);
    glUniform3f(terrain_.uCam, cam.pos.x, cam.pos.y, cam.pos.z);
    glUniform1f(terrain_.uFogK, fogK_);
    glUniform1i(terrain_.uShadowOn, shadowActive_ ? 1 : 0);
    glUniform1f(terrain_.uShadowTexel, texel);
    const float maxTerr = (float)((int)(q_.viewDist / CHUNK_SIZE) + 2) * CHUNK_SIZE;
    for (const auto& kv : cm.chunks()) {
        const Chunk& c = kv.second;
        if (!c.vao[0]) continue;
        float x0 = (float)(c.cx * CHUNK_SIZE), z0 = (float)(c.cz * CHUNK_SIZE);
        float cdx = std::max(std::max(x0 - cam.pos.x, 0.f), cam.pos.x - (x0 + CHUNK_SIZE));
        float cdz = std::max(std::max(z0 - cam.pos.z, 0.f), cam.pos.z - (z0 + CHUNK_SIZE));
        float d2 = cdx * cdx + cdz * cdz;
        if (d2 > maxTerr * maxTerr) continue;
        if (!frustumAABB(fr, x0, c.minY, z0, x0 + CHUNK_SIZE, c.maxY, z0 + CHUNK_SIZE)) continue;
        float d = std::sqrt(d2);
        int lod = d < q_.viewDist * 0.28f ? 0 : (d < q_.viewDist * 0.6f ? 1 : 2);
        glBindVertexArray(c.vao[lod]);
        glDrawElements(GL_TRIANGLES, (GLsizei)c.icount[lod], GL_UNSIGNED_SHORT, (const void*)(uintptr_t)c.ioff[lod]);
        lastDrawCalls_++;
    }

    glUseProgram(props_.prog);
    glUniformMatrix4fv(props_.uVP, 1, GL_FALSE, vp.m);
    glUniformMatrix4fv(props_.uLightVP, 1, GL_FALSE, lightBias.m);
    glUniform1f(props_.uTime, time_);
    glUniform1i(props_.uTex, 0);
    glUniform1i(props_.uShadow, 1);
    glUniform3f(props_.uSunDir, kSunDir.x, kSunDir.y, kSunDir.z);
    glUniform3f(props_.uSunCol, kSunCol[0], kSunCol[1], kSunCol[2]);
    glUniform3f(props_.uAmbTop, kAmbTop[0], kAmbTop[1], kAmbTop[2]);
    glUniform3f(props_.uAmbBot, kAmbBot[0], kAmbBot[1], kAmbBot[2]);
    glUniform3f(props_.uFogCol, kFogCol[0], kFogCol[1], kFogCol[2]);
    glUniform3f(props_.uCam, cam.pos.x, cam.pos.y, cam.pos.z);
    glUniform1f(props_.uFogK, fogK_);
    glUniform1i(props_.uShadowOn, shadowActive_ ? 1 : 0);
    glUniform1f(props_.uShadowTexel, texel);
    drawGroups(lists_, false);
    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);

    if (scaled) {
        GLenum att = GL_DEPTH_ATTACHMENT;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &att);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, rw, rh, 0, 0, W_, H_, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void Renderer::uiBegin() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W_, H_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(ui_.prog);
    glUniform2f(ui_.uScreen, (float)W_, (float)H_);
    glUniform1i(ui_.uTex, 0);
    glBindVertexArray(emptyVao_);
}

void Renderer::uiShape(int shape, float x, float y, float w, float h, float r, float g, float b, float a, float param, GLuint tex) {
    glUniform4f(ui_.uRect, x, y, w, h);
    glUniform2f(ui_.uSize, w, h);
    glUniform4f(ui_.uUV, 0.f, 0.f, 1.f, 1.f);
    glUniform4f(ui_.uColor, r, g, b, a);
    glUniform1i(ui_.uShape, shape);
    glUniform1f(ui_.uParam, param);
    if (shape == 3) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Renderer::uiEnd() {
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

namespace {

struct GlyphRow { char c; unsigned char r[7]; };

const GlyphRow kGlyphs[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}}, {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}}, {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}}, {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}}, {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}}, {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}}, {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}}, {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}}, {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04}}, {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}}, {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}}, {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}}, {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}}, {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}}, {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {':', {0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00}}, {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}}, {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}}, {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
};

int glyphIndex(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c < 32 || c > 95) return 0;
    return c - 32;
}

}  // namespace

void Renderer::buildFont() {
    const int cell = 16, cols = 8, dim = cell * cols;
    std::vector<unsigned char> a((size_t)dim * dim, 0);
    for (const GlyphRow& g : kGlyphs) {
        int idx = glyphIndex(g.c);
        int cx = (idx % cols) * cell, cy = (idx / cols) * cell;
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (g.r[row] & (1 << (4 - col)))
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++)
                            a[(size_t)(cy + 1 + row * 2 + dy) * dim + (size_t)(cx + 2 + col * 2 + dx)] = 255;
    }
    std::vector<unsigned char> px((size_t)dim * dim * 4);
    for (int y = 0; y < dim; y++)
        for (int x = 0; x < dim; x++) {
            auto at = [&](int xx, int yy) -> int {
                if (xx < 0 || yy < 0 || xx >= dim || yy >= dim) return 0;
                return a[(size_t)yy * dim + (size_t)xx];
            };
            int v = at(x, y) * 4 + at(x - 1, y) + at(x + 1, y) + at(x, y - 1) + at(x, y + 1);
            v = std::min(255, v * 3 / 4);
            size_t o = ((size_t)y * dim + (size_t)x) * 4;
            px[o] = px[o + 1] = px[o + 2] = 255;
            px[o + 3] = (unsigned char)v;
        }
    glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, dim, dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Renderer::uiSprite(GLuint tex, float x, float y, float w, float h, float r, float g, float b, float a, float u0, float v0, float u1, float v1) {
    glUniform4f(ui_.uRect, x, y, w, h);
    glUniform2f(ui_.uSize, w, h);
    glUniform4f(ui_.uUV, u0, v0, u1 - u0, v1 - v0);
    glUniform4f(ui_.uColor, r, g, b, a);
    glUniform1i(ui_.uShape, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

float Renderer::textWidth(const char* s, float h) const {
    size_t n = std::strlen(s);
    return n ? ((float)n - 1.f) * h * 0.76f + h * 0.66f : 0.f;
}

void Renderer::uiText(const char* s, float x, float y, float h, float r, float g, float b, float a, int align, bool shadow) {
    float w = textWidth(s, h);
    float x0 = align == 1 ? x - w * 0.5f : (align == 2 ? x - w : x);
    for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
        float off = pass == 0 ? h * 0.07f : 0.f;
        for (size_t i = 0; s[i]; i++) {
            int idx = glyphIndex(s[i]);
            if (idx == 0) continue;
            float u0 = (float)(idx % 8) / 8.f, v0 = (float)(idx / 8) / 8.f;
            float gx = x0 + (float)i * h * 0.76f - h * 0.125f + off;
            if (pass == 0) uiSprite(fontTex_, gx, y + off, h, h, 0.f, 0.f, 0.f, a * 0.45f, u0, v0, u0 + 0.125f, v0 + 0.125f);
            else uiSprite(fontTex_, gx, y, h, h, r, g, b, a, u0, v0, u0 + 0.125f, v0 + 0.125f);
        }
    }
}
