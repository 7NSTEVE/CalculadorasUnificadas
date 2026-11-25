#include <metal_stdlib>
using namespace metal;

// El color va como uniforme por trazo y no por vértice. `origen` permite
// renderizar un tile/viewport del documento sin trasladar ni duplicar la
// geometría vectorial almacenada en el núcleo.
struct Uniformes {
    float2 tamano;  // viewport en puntos lógicos del documento
    float2 origen;  // esquina superior izquierda del viewport en el documento
    float4 color;   // RGBA premultiplicado
};

struct SalidaVertice {
    float4 posicion [[position]];
};

vertex SalidaVertice vertice_tinta(uint id [[vertex_id]],
                                   constant float2 *vertices [[buffer(0)]],
                                   constant Uniformes &u [[buffer(1)]]) {
    const float2 p = vertices[id] - u.origen;
    // Puntos lógicos con el origen arriba a la izquierda, como UIKit, a NDC de
    // Metal. El viewport puede ser una página completa o un tile visible.
    const float2 ndc = float2((p.x / u.tamano.x) * 2.0 - 1.0,
                              1.0 - (p.y / u.tamano.y) * 2.0);
    SalidaVertice salida;
    salida.posicion = float4(ndc, 0.0, 1.0);
    return salida;
}

fragment float4 fragmento_tinta(SalidaVertice entrada [[stage_in]],
                                constant Uniformes &u [[buffer(1)]]) {
    return u.color;
}

struct SalidaTextura {
    float4 posicion [[position]];
    float2 uv;
};

vertex SalidaTextura vertice_textura(uint id [[vertex_id]]) {
    const float2 posiciones[4] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0,  1.0)
    };
    const float2 uvs[4] = {
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 0.0)
    };
    SalidaTextura salida;
    salida.posicion = float4(posiciones[id], 0.0, 1.0);
    salida.uv = uvs[id];
    return salida;
}

fragment float4 fragmento_textura(SalidaTextura entrada [[stage_in]],
                                  texture2d<float> textura [[texture(0)]]) {
    constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::nearest);
    return textura.sample(s, entrada.uv);
}
