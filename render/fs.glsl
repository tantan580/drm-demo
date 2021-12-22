precision mediump float;
uniform float time;
uniform vec3 resolution;
void main()
{
    //gl_FragColor = vec4(0.9686, 0.4745, 0.0118, 1.0);
    vec2 p = (gl_FragCoord.xy - 0.5* resolution.xy) / min(resolution.x, resolution.y);
    vec2 t = vec2(gl_FragCoord.xy / resolution.xy);

    vec3 c = vec3(0);
    float y=0.;
    float x=0.;
    for(int i = 0; i < 200; i++) {
            float t = float(i) * time * 0.5;
             x = 1. * cos(0.007*t);
             y = 1. * sin(t * 0.007);
            vec2 o = 0.3 * vec2(x, y);
            float r = fract(y+x);
            float g = 1. - r;
            c += 0.0007 / (length(p-o)) * vec3(r, g, x+y);
    }

    gl_FragColor = vec4(c, 1);
}