precision mediump float;

uniform sampler2D texture;
varying vec4 v_color;
varying vec4 v_texcoord;
varying vec4 v_clipping;

void main () {
	if (v_clipping.x > 1.0 || v_clipping.y > 1.0 || v_clipping.x < 0.0 || v_clipping.y < 0.0)
		discard;
	gl_FragColor = v_color * vec4 (1.0, 1.0, 1.0, texture2D (texture, v_texcoord.xy).a);
}
