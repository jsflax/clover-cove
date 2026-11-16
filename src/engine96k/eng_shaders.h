#ifndef ENG96K_SHADERS_H
#define ENG96K_SHADERS_H

// ============================================================================
// Clover Cove Unified Shader Library (Apple Metal + Android OpenGL ES 3.0)
//   - 4x MSAA Opaque + Cel Ink Outlines + Hardware-Instanced Grass
//   - 8-Tap IGN-Rotated Vogel-Disk Soft Shadows + Normal-Offset Bias
//   - Stylized Puffball Foliage + Procedural 3D Micro-Bump Normals
//   - Per-Pixel Rounded Stone Plaza/Ramp, Scalloped Roof Shingles & Wet Sand Glitter
//   - Depth-Buffer Water (Beer-Lambert Absorption, Contact Foam & Ring Ripples)
//   - Soft Particles & Terrain-Conforming SDF Character/Prop Contact Shadows
//   - Paired Symmetric Crease SSAO + Tilt-Shift Miniature Bokeh + 3-Mip Karis Bloom + AgX
// ============================================================================

#define SH_SHARED_CORE \
"float hash21(F2 p){" \
"p=FRAC(p*F2(123.34,456.21));" \
"p += dot(p,p+45.32);" \
"return FRAC(p.x*p.y);" \
"}" \
"float ign21(F2 px){" \
"return FRAC(52.9829189*FRAC(dot(px,F2(0.06711056,0.00583715))));" \
"}" \
"float rsmooth(float hi,float lo,float x){" \
"return 1.0-smoothstep(lo,hi,x);" \
"}" \
"float sd_rbox2(F2 p,F2 b,float r){" \
"F2 q=abs(p)-b+F2(r,r);" \
"return length(max(q,F2(0.0,0.0)))+min(max(q.x,q.y),0.0)-r;" \
"}" \
"float vnoise(F2 p){" \
"F2 i=floor(p),f=FRAC(p);" \
"f=f*f*(3.0-2.0*f);" \
"float a=hash21(i),b=hash21(i+F2(1.0,0.0));" \
"float c=hash21(i+F2(0.0,1.0)),d=hash21(i+F2(1.0,1.0));" \
"return LERP(LERP(a,b,f.x),LERP(c,d,f.x),f.y);" \
"}" \
"float fbm4(F2 p){" \
"float v=0.0,a=0.5;" \
"for(int i=0;i < 4;i++){" \
"v += a*vnoise(p);" \
"p=F2(p.x*1.62-p.y*1.18,p.x*1.18+p.y*1.62);" \
"a *= 0.5;" \
"}" \
"return v;" \
"}" \
"float voronoi2(F2 p){" \
"F2 n=floor(p),f=FRAC(p);" \
"float md=8.0;" \
"for(int j=-1;j<=1;j++){" \
"for(int i=-1;i<=1;i++){" \
"F2 g=F2(float(i),float(j));" \
"F2 o=F2(hash21(n+g),hash21(n+g+F2(17.1,31.7)));" \
"F2 r=g+0.5+0.42*sin(6.28318*o)-f;" \
"md=min(md,dot(r,r));" \
"}" \
"}" \
"return sqrt(md);" \
"}" \
"F3 apply_world_curve(F3 wp,F4 zen_pad,F4 ref_xz){" \
"if(zen_pad.w > 0.0001){" \
"F2 cd=wp.xz-ref_xz.xy;" \
"wp.y -= zen_pad.w*(cd.y*cd.y+0.28*cd.x*cd.x);" \
"}" \
"return wp;" \
"}" \
"F4 gen_proc_tex(F2 uv,int kind){" \
"F2 p=uv*8.0;" \
"if(kind==0){" \
"F2 tile=FRAC(uv*4.0);" \
"float bevel=smoothstep(0.0,0.055,tile.x)*smoothstep(0.0,0.055,1.0-tile.x)" \
"*smoothstep(0.0,0.055,tile.y)*smoothstep(0.0,0.055,1.0-tile.y);" \
"float w=fbm4(p*1.2+F2(fbm4(p),fbm4(p+F2(5.2,1.3)))*1.8);" \
"float vein=pow(SAT(1.0-abs(sin((p.x+p.y+w*4.5)*2.2))),6.0);" \
"float gold_inlay=smoothstep(0.028,0.012,abs(tile.x-0.5)*abs(tile.y-0.5)*4.0)*0.65;" \
"F3 col=LERP(F3(0.94,0.91,0.85),F3(0.75,0.73,0.77),vein*0.50);" \
"col=LERP(col*0.68,col,bevel);" \
"col=LERP(col,F3(0.95,0.78,0.36),SAT(gold_inlay));" \
"return F4(col,bevel*0.82+vein*0.18);" \
"} else if(kind==1){" \
"float n1=fbm4(p*1.5),n2=fbm4(p*4.2+F2(3.1,7.4));" \
"float strata=0.5+0.5*sin(uv.y*37.699+n1*5.0);" \
"F3 grass=LERP(F3(0.18,0.48,0.14),F3(0.46,0.74,0.22),SAT(n1*1.25));" \
"grass=LERP(grass,F3(0.62,0.80,0.28),smoothstep(0.62,0.88,n2)*0.55);" \
"return F4(grass,strata*0.6+n2*0.4);" \
"} else {" \
"float c1=voronoi2(uv*6.0),c2=voronoi2(uv*12.0+F2(2.3,4.1));" \
"float caust=pow(SAT(c1*1.15),3.2)+0.45*pow(SAT(c2),3.0);" \
"float damask=0.5+0.5*sin(uv.x*50.265)*sin(uv.y*50.265);" \
"float cloud=fbm4(p);" \
"return F4(SAT(caust),damask,cloud,hash21(uv*256.0));" \
"}" \
"}" \
"float island_cumulus(F2 cuv){" \
"float puff=1.0-voronoi2(cuv*2.1);" \
"float puff2=1.0-voronoi2(cuv*4.3+F2(1.7,3.1));" \
"float base=fbm4(cuv*1.35);" \
"return base*0.52+puff*0.34+puff2*0.14;" \
"}" \
"F3 eval_sky_dome(F3 dir,F3 sun_dir,float time_s,F3 sky_zen,F3 sky_hor,float sky_mode){" \
"F3 d=normalize(dir);" \
"float sdot=dot(d,sun_dir);" \
"float night=SAT(sky_mode-2.0);" \
"float i_up=SAT((d.y+0.21)*0.94);" \
"F3 mid_sky=LERP(sky_hor,sky_zen,0.44)*(1.15-0.22*night);" \
"F3 sky=LERP(sky_hor*(1.08-0.10*night),mid_sky,smoothstep(0.0,0.34,i_up));" \
"sky=LERP(sky,sky_zen,smoothstep(0.28,0.92,i_up));" \
"F3 glow_c=LERP(F3(1.22,0.88,0.50),F3(0.62,0.78,1.18),night);" \
"sky += glow_c*pow(SAT(sdot),6.5)*(0.48-0.20*night);" \
"sky += LERP(F3(1.85,1.58,1.08),F3(1.35,1.45,1.75),night)*smoothstep(0.9960,0.9990,sdot)*2.6;" \
"if(night > 0.08&&d.y > -0.10){" \
"F2 sp=d.xz/(max(d.y,-0.10)+0.46)*68.0;" \
"F2 si=floor(sp),sf=FRAC(sp)-0.5;" \
"float sh=hash21(si);" \
"if(sh > 0.958){" \
"float tw=0.58+0.42*sin(time_s*3.6+sh*95.0);" \
"F3 st_c=LERP(F3(1.05,1.18,1.55),F3(1.45,1.25,0.95),FRAC(sh*17.3));" \
"sky += st_c*rsmooth(0.18,0.02,length(sf))*night*tw*2.5*smoothstep(-0.08,0.14,d.y);" \
"}" \
"}" \
"if(d.y > -0.20&&d.y < 0.50){" \
"F2 cuv=d.xz/(abs(d.y+0.21)+0.28)*0.88+F2(time_s*0.0085,time_s*0.003);" \
"float cn=island_cumulus(cuv);" \
"float cn_sun=island_cumulus(cuv+sun_dir.xz*0.065);" \
"float band=smoothstep(-0.19,-0.05,d.y)*rsmooth(0.48,0.12,d.y);" \
"float mask=smoothstep(0.48,0.64,cn)*band;" \
"float rim=SAT((cn-cn_sun)*5.8+0.56);" \
"F3 c_lit=LERP(LERP(F3(1.18,1.15,1.10),F3(1.38,0.76,0.54),SAT(night*1.8)),F3(0.16,0.23,0.40),SAT(night*1.3-0.3));" \
"F3 c_shd=LERP(F3(0.62,0.80,0.96),F3(0.07,0.11,0.22),night);" \
"F3 c_col=LERP(c_shd,c_lit,SAT(rim*0.68+(cn-0.45)*1.45));" \
"sky=LERP(sky,c_col,mask*(0.90-0.32*night));" \
"}" \
"float sea_h=rsmooth(-0.20,-0.34,d.y);" \
"F3 deep_h=LERP(F3(0.04,0.48,0.68),F3(0.02,0.07,0.16),night);" \
"float sun_path=pow(SAT(dot(normalize(F2(d.x,d.z)),normalize(F2(sun_dir.x,sun_dir.z)))),24.0)*smoothstep(-0.34,-0.18,d.y);" \
"deep_h += glow_c*sun_path*(0.28-0.12*night);" \
"sky=LERP(sky,deep_h,sea_h*0.92);" \
"return sky;" \
"}" \
"F3 eval_villager_face(F2 uv,F3 skin_base,float face_style){" \
"if(uv.x < 0.0) return skin_base;" \
"F3 col=skin_base;float ax=abs(uv.x-0.5);" \
"if(face_style > 12.5){ float muz=smoothstep(1.0,0.70,length(F2(ax/0.175,(uv.y-0.30)/0.155)));col=LERP(col,F3(0.98,0.95,0.88),muz*0.92);}" \
"float blush=smoothstep(1.0,0.18,length(F2((ax-0.255)/0.088,(uv.y-0.355)/0.056)));" \
"col=LERP(col,F3(1.0,0.42,0.46),blush*0.46);" \
"if(face_style < 10.5){" \
"float nt=SAT((0.468-uv.y)/0.112),nw=0.004+0.032*nt;" \
"col=LERP(col,F3(0.95,0.25,0.10),smoothstep(nw+0.008,nw-0.003,ax)*smoothstep(0.352,0.362,uv.y)*smoothstep(0.472,0.458,uv.y)*0.96);" \
"} else if(face_style < 11.5){" \
"col=LERP(col,F3(1.0,0.45,0.58),smoothstep(1.0,0.65,length(F2(ax/0.038,(uv.y-0.375)/0.030)))*0.95);" \
"float wh1=smoothstep(0.011,0.003,abs(uv.y-0.36-(ax-0.32)*0.12))*step(0.28,ax)*step(ax,0.43);" \
"float wh2=smoothstep(0.011,0.003,abs(uv.y-0.31+(ax-0.32)*0.14))*step(0.28,ax)*step(ax,0.43);" \
"col=LERP(col,F3(0.22,0.16,0.18),SAT(wh1+wh2)*0.75);" \
"} else if(face_style > 12.5){" \
"col=LERP(col,F3(0.09,0.05,0.05),smoothstep(1.0,0.72,length(F2(ax/0.048,(uv.y-0.375)/0.036))));" \
"col=LERP(col,F3(0.95,0.95,1.0),smoothstep(0.35,0.12,length(F2(ax/0.048,(uv.y-0.388)/0.036)))*0.65);" \
"}" \
"float m_w=(face_style > 11.5&&face_style < 12.5)?0.125:(face_style < 10.5?0.060:0.072);" \
"float m_curve=(face_style > 10.5&&face_style < 11.5)?(0.255+(ax-0.036)*(ax-0.036)*10.0):(face_style < 10.5?(0.252+ax*ax*9.5):(0.242+ax*ax*2.8));" \
"col=LERP(col,F3(0.14,0.06,0.05),smoothstep(0.017,0.006,abs(uv.y-m_curve))*step(ax,m_w)*0.94);" \
"if(face_style < 10.5){" \
"F2 sp_e=F2((ax-0.182)/0.088,(uv.y-0.472)/0.108);" \
"float sd=length(sp_e);" \
"if(sd < 1.04){" \
"F3 ec=LERP(F3(0.98,0.97,0.97),F3(0.16,0.18,0.22),smoothstep(1.02,0.86,length(F2((ax-0.172)/0.062,(uv.y-0.472)/0.104))));" \
"ec=LERP(ec,F3(0.02,0.02,0.03),smoothstep(1.02,0.80,length(F2((ax-0.172)/0.035,(uv.y-0.472)/0.068))));" \
"ec=LERP(ec,F3(1.20,1.20,1.22),smoothstep(0.24,0.10,length(sp_e-F2(-0.18,0.34)))*0.78);" \
"col=LERP(col,ec,smoothstep(1.04,0.88,sd));" \
"}" \
"float lid_y=0.568-1.25*(ax-0.178)*(ax-0.178)+smoothstep(0.235,0.292,ax)*0.045;" \
"return LERP(col,F3(0.08,0.04,0.04),SAT(smoothstep(0.019,0.006,abs(uv.y-lid_y))*smoothstep(0.095,0.112,ax)*smoothstep(0.298,0.278,ax)));" \
"}" \
"F2 ep=F2((ax-0.188)/0.072,(uv.y-0.505)/0.102);" \
"float ed=length(ep);" \
"if(ed < 1.05){" \
"F3 iris_lo=(face_style > 12.5)?F3(0.52,0.28,0.14):((face_style > 11.5)?F3(0.16,0.68,0.42):((face_style > 10.5)?F3(0.95,0.68,0.16):F3(0.18,0.52,0.92)));" \
"F3 ec=LERP(F3(0.05,0.04,0.08),iris_lo,smoothstep(0.15,-0.85,ep.y)*0.85);" \
"ec=LERP(ec,F3(1.30,1.30,1.35),SAT(smoothstep(0.36,0.18,length(ep-F2(-0.22,0.36)))+smoothstep(0.20,0.08,length(ep-F2(0.24,-0.34)))*0.85));" \
"col=LERP(col,ec,smoothstep(1.04,0.86,ed));" \
"}" \
"float brow=smoothstep(0.014,0.004,abs(uv.y-(0.655-0.6*(ax-0.185)*(ax-0.185))))*step(0.11,ax)*step(ax,0.26);" \
"return LERP(col,F3(0.16,0.10,0.10),brow*0.65);" \
"}" \
"F3 eval_aloha_shirt(F2 uv,F3 base){" \
"F2 p=FRAC(F2(uv.x*5.0,uv.y*2.8))-0.5,q=FRAC(F2(uv.x*5.0+0.5,uv.y*2.8+0.5))-0.5;" \
"float d1=length(p*F2(1.0,2.7)),d2=length(F2(p.x*0.5+p.y*0.866,-p.x*0.866+p.y*0.5)*F2(1.0,2.7)),d3=length(F2(p.x*0.5-p.y*0.866,p.x*0.866+p.y*0.5)*F2(1.0,2.7));" \
"float grid=step(0.035,abs(FRAC(q.x*4.5)-0.5))*step(0.035,abs(FRAC(q.y*4.5)-0.5));" \
"return LERP(base,F3(0.06,0.30,0.68),SAT(smoothstep(0.32,0.20,min(d1,min(d2,d3)))+smoothstep(0.28,0.21,length(q))*(0.35+0.65*grid))*0.88);" \
"}" \
"F3 eval_roof_shingles(F2 uv,F3 base,F3 N_in,OUT_F3(N_out),OUT_F1(r_out)){" \
"float row=floor(uv.y*5.0);" \
"F2 st=F2(FRAC(uv.x*4.5+0.5*FRAC(row*0.5)),FRAC(uv.y*5.0));" \
"float seam_y=smoothstep(0.0,0.14,st.y+0.28*(st.x-0.5)*(st.x-0.5));" \
"float seam_x=smoothstep(0.0,0.07,st.x)*smoothstep(0.0,0.07,1.0-st.x);" \
"float tile_var=hash21(F2(floor(uv.x*4.5+0.5*FRAC(row*0.5)),row));" \
"N_out=normalize(N_in+F3((st.x-0.5)*0.22,(0.35-st.y)*0.32,0.0));r_out=LERP(0.68,0.44,seam_y);" \
"return base*(0.90+0.18*tile_var)*LERP(0.62,1.06,seam_y*(0.75+0.25*seam_x));" \
"}" \
"F3 bloom_karis_tap(F3 c,float do_thresh){" \
"c=max(c,F3(0.0,0.0,0.0));" \
"if(do_thresh > 0.5){" \
"float luma=max(c.r,max(c.g,c.b));" \
"float knee=smoothstep(1.08,2.45,luma);" \
"c *= knee/(1.0+luma*0.32);" \
"}" \
"return c;" \
"}" \
"F3 agx_tonemap(F3 x){" \
"x=max(x,F3(0.0,0.0,0.0));" \
"F3 a=x*(x+0.0245786)-0.000090537;" \
"F3 b=x*(0.983729*x+0.4329510)+0.238081;" \
"F3 c=SAT(a/b);" \
"float luma=dot(c,F3(0.2126,0.7152,0.0722));" \
"c=LERP(F3(luma,luma,luma),c,1.15);" \
"return pow(SAT(c),F3(1.0/2.2,1.0/2.2,1.0/2.2));" \
"}"

// ============================================================================
// APPLE METAL SHADING LANGUAGE (MSL) COMPLETE SOURCE
// ============================================================================
#ifdef __APPLE__
static const char *k_metal_shader_src =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"#define F2 float2\n"
"#define F3 float3\n"
"#define F4 float4\n"
"#define LERP mix\n"
"#define SAT saturate\n"
"#define FRAC fract\n"
"#define OUT_F3(x) thread float3 &x\n"
"#define OUT_F1(x) thread float &x\n"
SH_SHARED_CORE
"struct SceneUniforms {"
"float4x4 view;"
"float4x4 proj;"
"float4x4 view_proj;"
"float4x4 inv_view_proj;"
"float4x4 light_view_proj;"
"float4 cam_pos_time;"
"float4 sun_dir_intensity;"
"float4 sun_color_fog;"
"float4 sky_zenith_pad;"
"float4 sky_horizon_pad;"
"float4 grid_params;"
"float4 hover_cursor;"
"float4 screen_params;"
"float4 sun_screen_pos;"
"float4 tile_overlay[42];"
"};"
"struct DrawUniforms {"
"float4x4 model;"
"float4 tint_outline;"
"float4 hair_color;"
"float4 custom_params;"
"float4x4 bones[16];"
"};"
"struct VsIn {"
"float3 pos [[attribute(0)]];"
"float3 nrm [[attribute(1)]];"
"float2 uv  [[attribute(2)]];"
"uchar4 col [[attribute(3)]];"
"uchar4 par [[attribute(4)]];"
"uchar4 jnt [[attribute(5)]];"
"uchar4 wgt [[attribute(6)]];"
"};"
"struct GrassInst { float4 pos_scale;float4 rot_color;};"
"struct FsQuadOut { float4 pos [[position]];float2 uv;};"
"inline float linear_eye_z(float z_buf,constant SceneUniforms &su){"
"return su.proj[3][2]/(z_buf+su.proj[2][2]);"
"}"
"vertex FsQuadOut vs_fullscreen(uint vid [[vertex_id]]){"
"FsQuadOut o;"
"float2 uv=float2((vid << 1) & 2,vid & 2);"
"o.uv=uv;"
"o.pos=float4(uv*float2(2.0,-2.0)+float2(-1.0,1.0),0.0,1.0);"
"return o;"
"}"
"fragment float4 fs_gen_tex(FsQuadOut in [[stage_in]],constant int &kind [[buffer(0)]]){"
"return gen_proc_tex(in.uv,kind);"
"}"
"fragment float4 fs_sky(FsQuadOut in [[stage_in]],constant SceneUniforms &su [[buffer(0)]]){"
"float2 ndc=float2(in.uv.x*2.0-1.0,1.0-in.uv.y*2.0);"
"float4 wp=su.inv_view_proj*float4(ndc,1.0,1.0);"
"float3 dir=wp.xyz/wp.w-su.cam_pos_time.xyz;"
"return float4(eval_sky_dome(dir,su.sun_dir_intensity.xyz,su.cam_pos_time.w,su.sky_zenith_pad.rgb,su.sky_horizon_pad.rgb,su.sky_horizon_pad.w),1.0);"
"}"
"float4 skin_pos(VsIn v,constant DrawUniforms &du,thread float3 &out_n){"
"float3 p=v.pos,n=v.nrm;"
"if(du.custom_params.w > 0.5){"
"float4 w=float4(v.wgt)/255.0;"
"float4x4 skin=du.bones[v.jnt.x]*w.x+du.bones[v.jnt.y]*w.y;"
"p=(skin*float4(p,1.0)).xyz;"
"n=normalize((skin*float4(n,0.0)).xyz);"
"}"
"out_n=normalize((du.model*float4(n,0.0)).xyz);"
"return du.model*float4(p,1.0);"
"}"
"vertex float4 vs_shadow(VsIn v [[stage_in]],constant SceneUniforms &su [[buffer(0)]],constant DrawUniforms &du [[buffer(1)]]){"
"float3 wn;"
"float4 wp=skin_pos(v,du,wn);"
"if(v.par.x==2){ wp.x += sin(su.cam_pos_time.w*2.1+wp.z*0.7+wp.y*0.5)*0.065;}"
"return su.light_view_proj*wp;"
"}"
"struct OutlineOut { float4 pos [[position]];float4 col;};"
"vertex OutlineOut vs_outline(VsIn v [[stage_in]],constant SceneUniforms &su [[buffer(0)]],constant DrawUniforms &du [[buffer(1)]]){"
"OutlineOut o;"
"float3 wn;"
"float4 wp=skin_pos(v,du,wn);"
"float ow=(float(v.par.w)/255.0)*du.tint_outline.w;"
"float3 cwp=apply_world_curve(wp.xyz,su.sky_zenith_pad,su.tile_overlay[41]);"
"float4 cp=su.view_proj*float4(cwp,1.0);"
"if(ow<=0.001){ o.pos=float4(0,0,-2,1);o.col=float4(0);return o;}"
"float3 cn=(su.view_proj*float4(wn,0.0)).xyz;"
"float2 dir2=normalize(cn.xy+float2(1e-5));"
"float px_scale=clamp(cp.w,1.2,20.0);"
"cp.xy += dir2*ow*2.15*su.screen_params.zw*px_scale;"
"cp.z += 0.0022*cp.w;"
"o.pos=cp;"
"o.col=float4(0.11,0.072,0.085,1.0);"
"return o;"
"}"
"fragment float4 fs_outline(OutlineOut in [[stage_in]]){ return in.col;}"
"struct MainVarying {"
"float4 pos [[position]];"
"float3 wpos;"
"float3 wnrm;"
"float2 uv;"
"float4 vcol;"
"float2 rm;"
"uint mat_id [[flat]];"
"};"
"vertex MainVarying vs_main(VsIn v [[stage_in]],constant SceneUniforms &su [[buffer(0)]],constant DrawUniforms &du [[buffer(1)]]){"
"MainVarying o;"
"float3 wn;"
"float4 wp=skin_pos(v,du,wn);"
"uint mat_id=v.par.x;"
"if(mat_id==2){ wp.x += sin(su.cam_pos_time.w*2.1+wp.z*0.7+wp.y*0.5)*0.068;}"
"o.wpos=wp.xyz;"
"o.wnrm=wn;"
"o.uv=v.uv;"
"float3 lin_c=pow(max(float3(v.col.rgb)/255.0,float3(0.0)),float3(2.2));"
"o.vcol=float4(lin_c,float(v.col.a)/255.0);"
"o.rm=float2(float(v.par.y)/255.0,float(v.par.z)/255.0);"
"o.mat_id=mat_id;"
"float3 cwp=apply_world_curve(wp.xyz,su.sky_zenith_pad,su.tile_overlay[41]);"
"o.pos=su.view_proj*float4(cwp,1.0);"
"return o;"
"}"
"float sample_pcf_shadow(float3 wpos,float3 N,float2 frag_xy,constant SceneUniforms &su,depth2d<float> sh_tex,sampler sh_smp){"
"float4 lp=su.light_view_proj*float4(wpos+N*0.022,1.0);"
"float3 ndc=lp.xyz/lp.w;"
"float2 suv=float2(ndc.x*0.5+0.5,0.5-ndc.y*0.5);"
"if(suv.x < 0.01||suv.x > 0.99||suv.y < 0.01||suv.y > 0.99||ndc.z < 0.0||ndc.z > 1.0) return 1.0;"
"float z_ref=ndc.z-0.0018;"
"float ang=ign21(frag_xy)*6.283185;"
"float sum=0.0;"
"float rad=2.05/2048.0;"
"for(int i=0;i < 8;i++){"
"float r=sqrt((float(i)+0.5)*0.125)*rad;"
"float th=float(i)*2.399963+ang;"
"sum += sh_tex.sample_compare(sh_smp,suv+float2(cos(th),sin(th))*r,z_ref);"
"}"
"return sum*0.125;"
"}"
"fragment float4 fs_main(MainVarying in [[stage_in]],\n"
"constant SceneUniforms &su [[buffer(0)]],\n"
"constant DrawUniforms &du [[buffer(1)]],\n"
"texture2d<float> tex_marble [[texture(0)]],\n"
"texture2d<float> tex_grass  [[texture(1)]],\n"
"texture2d<float> tex_fabric [[texture(2)]],\n"
"depth2d<float>   tex_shadow [[texture(3)]],\n"
"sampler lin_smp [[sampler(0)]],\n"
"sampler sh_smp  [[sampler(1)]]){"
"int mat=int(in.mat_id);"
"float rough=in.rm.x;"
"float metal=in.rm.y;"
"float3 N=normalize(in.wnrm);"
"float3 V=normalize(su.cam_pos_time.xyz-in.wpos);"
"float3 L=normalize(su.sun_dir_intensity.xyz);"
"float3 H=normalize(L+V);"
"float3 albedo=in.vcol.rgb;"
"bool is_char=(du.custom_params.w > 0.5);"
"if(mat==0){"
"float2 muv=in.wpos.xz*0.22;"
"float4 m=tex_marble.sample(lin_smp,muv);"
"float mx=tex_marble.sample(lin_smp,muv+float2(0.004,0.0)).a,mz=tex_marble.sample(lin_smp,muv+float2(0.0,0.004)).a;"
"N=normalize(N+float3(m.a-mx,0.0,m.a-mz)*1.15);"
"albedo *= LERP(float3(0.85),m.rgb*1.12,0.85);rough=LERP(0.46,0.22,m.a);"
"} else if(mat==1){"
"float4 gr=tex_grass.sample(lin_smp,in.wpos.xz*0.18);"
"float2 ruv=float2(in.wpos.x*0.11+in.wpos.z*0.11,in.wpos.y*0.16);"
"float4 rk=tex_grass.sample(lin_smp,ruv);"
"float rk_y=tex_grass.sample(lin_smp,ruv+float2(0.0,0.006)).a;"
"float grass_mask=smoothstep(0.66,0.86,N.y)*smoothstep(-0.05,0.24,in.wpos.y);"
"float3 cliff_c=LERP(float3(0.32,0.22,0.15),float3(0.64,0.50,0.36),rk.a);"
"float3 cliff_N=normalize(N+float3(0.0,(rk.a-rk_y)*1.4,0.0));"
"float3 turf_c=gr.rgb*1.18;"
"float2 tc=FRAC(in.wpos.xz*0.55)-0.5;"
"float tri=step(abs(tc.x)*1.6+tc.y,0.18)*step(-0.22,tc.y);"
"turf_c=LERP(turf_c*float3(0.88,1.06,0.82),turf_c*float3(1.06,1.15,0.84),tri*0.34);"
"albedo=LERP(cliff_c,turf_c,grass_mask);N=normalize(LERP(cliff_N,N,grass_mask));"
"float north_plat=rsmooth(-3.0,-3.5,in.wpos.z)*rsmooth(8.8,8.2,abs(in.wpos.x));"
"float sand_m=rsmooth(0.40,0.24,in.wpos.y)*smoothstep(0.32,0.68,N.y)*(1.0-north_plat);"
"if(sand_m > 0.005){"
"float grain=vnoise(in.wpos.xz*6.5)*0.5+vnoise(in.wpos.xz*22.0)*0.5;"
"float ripple=0.5+0.5*sin(in.wpos.x*2.4+in.wpos.z*1.8+grain*1.6);"
"float wet_lip=rsmooth(0.30,0.06,in.wpos.y)*smoothstep(-0.08,0.05,in.wpos.y);"
"float glit=smoothstep(0.955,0.995,hash21(floor(in.wpos.xz*52.0)))*(1.0-wet_lip*0.4);"
"float3 sand_c=float3(0.93,0.79,0.54)*(0.93+0.13*ripple)+float3(0.45,0.42,0.35)*glit;"
"sand_c=LERP(sand_c,sand_c*float3(0.62,0.56,0.50),wet_lip*0.72);"
"albedo=LERP(albedo,sand_c,sand_m);rough=LERP(rough,LERP(0.76,0.16,wet_lip),sand_m);"
"N=normalize(N+float3(cos(in.wpos.x*2.4+in.wpos.z*1.8)*0.08*sand_m,0.0,0.0));"
"}"
"float d_pav=min(sd_rbox2(in.wpos.xz-float2(-1.60,1.80),float2(2.62,2.62),0.52),sd_rbox2(in.wpos.xz-float2(-5.52,-1.60),float2(1.24,2.04),0.24));"
"float pav_m=rsmooth(0.06,-0.04,d_pav)*smoothstep(0.38,0.52,in.wpos.y)*smoothstep(0.55,0.75,N.y);"
"if(pav_m > 0.005){"
"float2 puv=in.wpos.xz*0.28;"
"float4 pm=tex_marble.sample(lin_smp,puv);"
"float px=tex_marble.sample(lin_smp,puv+float2(0.005,0.0)).a,pz=tex_marble.sample(lin_smp,puv+float2(0.0,0.005)).a;"
"float curb=smoothstep(-0.14,-0.05,d_pav)*rsmooth(0.06,-0.02,d_pav);"
"float3 stone_c=LERP(pm.rgb*float3(0.96,0.89,0.77)*(0.92+0.14*pm.a),float3(0.84,0.73,0.58),curb*0.75);"
"albedo=LERP(albedo,stone_c,pav_m);rough=LERP(rough,LERP(0.54,0.30,pm.a),pav_m);"
"N=normalize(N+float3(pm.a-px,0.0,pm.a-pz)*(1.25*pav_m));"
"}"
"} else if(mat==2||mat==3){"
"float cam_d=length(su.cam_pos_time.xyz-in.wpos);"
"if(cam_d < 2.4){"
"float fade=smoothstep(1.2,2.4,cam_d);"
"float bayer=fract(dot(floor(in.pos.xy),float2(0.5,0.25))+fract(in.pos.x*0.125)*0.5);"
"if(fade < bayer) discard_fragment();"
"}"
"if(mat==2){"
"float4 lf=tex_grass.sample(lin_smp,in.wpos.xz*0.48+in.wpos.y*0.30);"
"float cl_p=1.0-voronoi2(in.wpos.xz*2.6+in.wpos.y*1.8);"
"albedo *= (0.84+0.30*lf.g)*(0.90+0.18*cl_p);"
"N=normalize(N+float3(lf.r-0.35,(cl_p-0.45)*0.35,lf.g-0.45)*0.22);"
"} else {"
"float bark=sin(in.uv.x*37.699+fbm4(in.wpos.xy*3.5)*3.2);"
"albedo *= (0.92+0.12*bark);N=normalize(N+float3(bark*0.12,0.0,-bark*0.08));"
"}"
"} else if(mat==4){"
"albedo=eval_villager_face(in.uv,albedo,du.custom_params.y);"
"} else if(mat==5){"
"albedo=pow(max(du.hair_color.rgb,float3(0.001)),float3(2.2))*(0.74+0.34*in.uv.y);"
"} else if(mat==6){"
"float4 fb=tex_fabric.sample(lin_smp,in.uv*2.5);"
"if(in.uv.x>=1.95){ albedo *= pow(max(du.tint_outline.rgb,float3(0.001)),float3(2.2));}"
"if(in.uv.x>=4.95){ albedo=eval_aloha_shirt(in.uv,albedo);}"
"albedo *= (0.90+0.16*fb.g);"
"} else if(mat==8){"
"if(in.vcol.a < 0.94){"
"float rim_f=pow(1.0-SAT(abs(dot(N,V))),2.2);"
"float hex_g=rsmooth(0.14,0.03,abs(voronoi2(in.wpos.xy*4.5+in.wpos.z*3.2+su.cam_pos_time.w*0.6)-0.34));"
"if(SAT(rim_f*0.88+hex_g*0.46)*in.vcol.a*1.35 < ign21(in.pos.xy)) discard_fragment();"
"return float4((albedo*du.tint_outline.rgb)*(1.65+1.85*rim_f+0.95*hex_g),1.0);"
"}"
"return float4(albedo*(2.4+1.5*du.custom_params.z),1.0);"
"} else if(mat==9){"
"float grain=vnoise(in.wpos.xz*6.5)*0.5+vnoise(in.wpos.xz*22.0)*0.5;"
"float ripple=0.5+0.5*sin(in.wpos.x*2.4+in.wpos.z*1.8+grain*1.6);"
"float wet=smoothstep(0.30,-0.06,in.wpos.y);"
"albedo=LERP(albedo*(0.92+0.14*ripple),albedo*float3(0.58,0.52,0.48),wet*0.78);rough=LERP(0.76,0.16,wet);"
"} else if(mat==10){"
"if(in.uv.x>=7.5){ float3 rn;float rr;albedo=eval_roof_shingles(in.uv-float2(8.0,0.0),albedo,N,rn,rr);N=rn;rough=rr;}"
"else { albedo *= (0.94+0.08*sin(in.uv.y*25.1327));}"
"}"
"float shadow=sample_pcf_shadow(in.wpos,N,in.pos.xy,su,tex_shadow,sh_smp)*(0.80+0.20*smoothstep(0.35,0.72,tex_fabric.sample(lin_smp,in.wpos.xz*0.025+su.cam_pos_time.w*0.012).b));"
"float ndl=dot(N,L);"
"float3 sun_col=su.sun_color_fog.rgb*su.sun_dir_intensity.w;"
"float3 bounce_c=float3(0.24,0.36,0.24)*SAT(-N.y*0.55+0.45)*su.sun_dir_intensity.w*0.13;"
"float3 lit=float3(0.0);"
"if(is_char){"
"float cel=smoothstep(-0.08,0.14,ndl*(0.38+0.62*shadow));"
"float3 sh_tint=albedo*(LERP(float3(0.42,0.40,0.62),su.sky_zenith_pad.rgb*0.55+float3(0.24,0.26,0.38),0.68)+bounce_c);"
"float3 diff=LERP(sh_tint,albedo*float3(1.06,1.00,0.94),cel);"
"if(mat==4){ diff += albedo*float3(0.18,0.05,0.04)*smoothstep(-0.2,0.25,ndl)*(1.0-cel);}"
"float spec_raw=pow(max(SAT(dot(N,H)),1e-4),exp2(LERP(4.0,9.0,1.0-rough)));"
"float spec=spec_raw*(metal > 0.4?1.65:0.16)*cel;"
"float3 spec_col=LERP(float3(1.0),albedo*1.6+float3(0.25),metal)*spec*sun_col*0.40;"
"if(mat==5){"
"float ring1=smoothstep(0.82,0.95,SAT(1.0-abs(N.y-0.50)))*cel;"
"spec_col += (albedo+float3(0.28,0.20,0.14))*ring1*0.24+su.sky_horizon_pad.rgb*smoothstep(0.86,0.97,SAT(1.0-abs(N.y-0.28)))*0.063;"
"}"
"float rim=pow(1.0-SAT(dot(N,V)),3.5)*SAT(dot(V,-L)*0.6+0.55);"
"lit=LERP(diff*1.15+spec_col+sun_col*(rim*0.26),float3(3.5,1.4,1.2),du.custom_params.x);"
"} else {"
"float wrap_d=SAT((ndl+0.25)/1.25)*shadow;"
"float3 amb=LERP(float3(0.11,0.13,0.18)+bounce_c,su.sky_zenith_pad.rgb*0.46+float3(0.15,0.19,0.25),N.y*0.5+0.5);"
"float spec=pow(max(SAT(dot(N,H)),1e-4),LERP(8.0,84.0,1.0-rough))*(1.0-rough)*shadow*0.46;"
"float sss=(mat==2)?(pow(SAT(dot(V,-L)),2.2)*0.52+0.22*SAT(N.y))*(0.45+0.55*shadow):0.0;"
"lit=albedo*(amb+sun_col*wrap_d*0.58+float3(0.95,0.88,0.38)*sss)+sun_col*spec;"
"}"
"for(int li=0;li < 8;li++){"
"float4 pl=su.tile_overlay[li];"
"if(pl.w > 0.05){"
"float3 lv=pl.xyz-in.wpos;"
"float att=pl.w/(1.0+dot(lv,lv)*0.28);"
"lit += (albedo*0.65+float3(0.35))*float3(1.25,0.78,0.34)*att*(0.45+0.55*SAT(dot(N,normalize(lv))));"
"}"
"}"
"if(su.hover_cursor.z > 0.5&&!is_char&&N.y > 0.65){"
"float hr=length(in.wpos.xz-su.hover_cursor.xy);"
"float hring=smoothstep(0.68,0.58,hr)*smoothstep(0.42,0.52,hr);"
"lit += float3(1.35,1.15,0.45)*hring*(0.75+0.25*sin(su.cam_pos_time.w*5.0));"
"}"
"float dist=length(su.cam_pos_time.xyz-in.wpos);"
"float fog=1.0-exp(-max(0.0,dist-15.0)*su.sun_color_fog.w);"
"lit=LERP(lit,su.tile_overlay[40].rgb,SAT(fog*0.68));"
"return float4(lit,in.vcol.a);"
"}"
"vertex MainVarying vs_grass(VsIn v [[stage_in]],uint iid [[instance_id]],\n"
"constant SceneUniforms &su [[buffer(0)]],\n"
"constant GrassInst *insts [[buffer(1)]]){"
"MainVarying o;"
"GrassInst gi=insts[iid];"
"float ca=cos(gi.rot_color.x),sa=sin(gi.rot_color.x);"
"float3 lp=v.pos*gi.pos_scale.w;"
"float3 rp=float3(lp.x*ca-lp.z*sa,lp.y,lp.x*sa+lp.z*ca);"
"float gust=sin(su.cam_pos_time.w*2.6+gi.pos_scale.x*0.35+gi.pos_scale.z*0.25+gi.rot_color.y);"
"float bend=v.uv.y*v.uv.y*(0.14+0.10*gust);"
"rp.x += bend;rp.y -= bend*0.25;"
"float3 wp=gi.pos_scale.xyz+rp;"
"o.wpos=wp;"
"o.wnrm=normalize(float3(-0.18*gust,0.96,0.1));"
"o.uv=v.uv;"
"float3 base_g=LERP(float3(0.11,0.36,0.08),float3(0.46,0.80,0.19),v.uv.y)*gi.rot_color.w;"
"if(gi.rot_color.z > 0.5&&v.uv.y > 0.68){"
"if(gi.rot_color.z > 2.5) base_g=float3(1.35,0.42,0.62);"
"else if(gi.rot_color.z > 1.5) base_g=float3(0.35,0.78,1.35);"
"else base_g=float3(1.35,0.95,0.25);"
"}"
"o.vcol=float4(base_g,1.0);"
"o.rm=float2(0.6,0.0);"
"o.mat_id=2u;"
"float3 cwp=apply_world_curve(wp,su.sky_zenith_pad,su.tile_overlay[41]);"
"o.pos=su.view_proj*float4(cwp,1.0);"
"return o;"
"}"
"fragment float4 fs_grass(MainVarying in [[stage_in]],constant SceneUniforms &su [[buffer(0)]],\n"
"depth2d<float> tex_shadow [[texture(0)]],sampler sh_smp [[sampler(0)]]){"
"float sh=sample_pcf_shadow(in.wpos,float3(0,1,0),in.pos.xy,su,tex_shadow,sh_smp);"
"float3 sun_c=su.sun_color_fog.rgb*su.sun_dir_intensity.w;"
"float3 gc=in.vcol.rgb;"
"float3 amb=su.sky_zenith_pad.rgb*0.40+float3(0.09,0.11,0.16);"
"float3 lit=gc*LERP(0.68,1.12,in.uv.y)*(amb+sun_c*(0.20+0.54*sh));"
"for(int li=0;li < 8;li++){"
"float4 pl=su.tile_overlay[li];"
"if(pl.w > 0.05){ float3 lv=pl.xyz-in.wpos;lit += gc*float3(1.2,0.75,0.32)*(pl.w/(1.0+dot(lv,lv)*0.30));}"
"}"
"return float4(lit,1.0);"
"}"
"vertex MainVarying vs_water(VsIn v [[stage_in]],constant SceneUniforms &su [[buffer(0)]]){"
"MainVarying o;"
"float3 wp=v.pos;"
"if(v.uv.x < 9.0){"
"wp.y += sin(su.cam_pos_time.w*2.3+wp.x*1.2)*cos(su.cam_pos_time.w*1.7+wp.z*1.4)*0.032;"
"}"
"o.wpos=wp;o.wnrm=v.nrm;o.uv=v.uv;"
"o.vcol=float4(float3(v.col.rgb)/255.0,float(v.col.a)/255.0);"
"o.rm=float2(0.0);"
"o.mat_id=0u;"
"float3 cwp=apply_world_curve(wp,su.sky_zenith_pad,su.tile_overlay[41]);"
"o.pos=su.view_proj*float4(cwp,1.0);"
"return o;"
"}"
"fragment float4 fs_water(MainVarying in [[stage_in]],constant SceneUniforms &su [[buffer(0)]],\n"
"texture2d<float> tex_fabric [[texture(0)]],\n"
"depth2d<float>   tex_depth  [[texture(1)]],\n"
"sampler lin_smp [[sampler(0)]]){"
"uint2 pix=min(uint2(in.pos.xy),uint2(tex_depth.get_width()-1u,tex_depth.get_height()-1u));"
"float scene_z=tex_depth.read(pix);"
"if(in.pos.z > scene_z) discard_fragment();"
"float wdepth=max(0.0,linear_eye_z(scene_z,su)-linear_eye_z(in.pos.z,su));"
"float t=su.cam_pos_time.w;"
"bool is_fall=(in.uv.x>=9.5);"
"float3 V=normalize(su.cam_pos_time.xyz-in.wpos);"
"float vdepth=wdepth*max(0.28,abs(V.y));"
"float2 uv1=is_fall?float2(in.uv.x*2.0,in.uv.y-t*1.8):(in.wpos.xz*0.22+float2(t*0.075,t*0.028));"
"float2 uv2=is_fall?float2(in.uv.x*3.5,in.uv.y*1.4-t*2.4):(in.wpos.xz*0.33-float2(t*0.055,-t*0.038));"
"float c1=tex_fabric.sample(lin_smp,uv1).r;"
"float c2=tex_fabric.sample(lin_smp,uv2).r;"
"float caust=(c1+c2)*0.65;"
"float rip_foam=0.0;"
"float2 rip_n=float2(0.0);"
"if(!is_fall){"
"for(int ri=8;ri<=9;ri++){"
"float4 rp=su.tile_overlay[ri];"
"if(rp.w > 0.05){"
"float2 dv=in.wpos.xz-rp.xz;"
"float rd=length(dv);"
"if(rd < 2.4){"
"float rw=sin(rd*15.0-t*5.2)*exp(-rd*1.35)*rp.w;"
"rip_foam += max(0.0,rw)*0.45;"
"rip_n += (dv/max(rd,0.05))*rw*0.16;"
"}"
"}"
"}"
"}"
"float3 N=normalize(in.wnrm+float3((c1-c2)*0.24+rip_n.x,0.0,(c2-0.4)*0.24+rip_n.y));"
"float3 L=normalize(su.sun_dir_intensity.xyz);"
"float3 sky_r=eval_sky_dome(reflect(-V,N),L,t,su.sky_zenith_pad.rgb,su.sky_horizon_pad.rgb,su.sky_horizon_pad.w);"
"float fres=0.06+0.58*pow(1.0-SAT(dot(N,V)),4.4);"
"float night=SAT(su.sky_horizon_pad.w-2.0);"
"if(is_fall){"
"float3 fall_c=LERP(float3(0.05,0.64,0.88),float3(0.24,0.88,0.96),SAT(caust))*(1.0-0.58*night);"
"float f_vein=smoothstep(0.38,0.72,caust)*0.80+smoothstep(0.60,0.86,c1)*0.45;"
"float f_ends=rsmooth(0.36,0.02,in.uv.y)*0.72+smoothstep(1.65,2.15,in.uv.y)*0.88+rsmooth(0.55,0.0,abs(in.wpos.y+0.1))*0.55;"
"float3 fcol=LERP(fall_c,float3(1.20,1.25,1.32)*(1.0-0.42*night),SAT(f_vein+f_ends));"
"return float4(fcol,0.93);"
"}"
"float3 body=LERP(float3(0.08,0.84,0.78),float3(0.02,0.56,0.80),smoothstep(0.04,0.58,vdepth));"
"body=LERP(body,float3(0.01,0.24,0.54),smoothstep(0.50,2.50,vdepth))*(1.0-0.62*night);"
"float caust_vis=exp(-vdepth*0.65)*(0.85-0.45*night);"
"float caust_net=smoothstep(0.34,0.82,caust);"
"float spec=pow(SAT(dot(N,normalize(L+V))),220.0)*4.5*su.sun_dir_intensity.w*0.30;"
"float contact_f=rsmooth(0.145,0.012,vdepth)*(0.62+0.38*sin(vdepth*34.0-t*3.2+caust*2.5));"
"float sh=in.vcol.r;"
"float w1=sin(sh*18.0-t*2.6)*0.5+0.5;"
"float surf=smoothstep(0.42,0.94,sh)*(0.32+0.56*smoothstep(0.64,0.88,w1))*rsmooth(0.85,0.08,vdepth);"
"float foam=SAT(contact_f*0.92+surf+smoothstep(0.66,0.78,caust)*0.42+rip_foam);"
"float3 col=LERP(body+float3(0.42,0.96,0.92)*caust_net*caust_vis,sky_r,fres)+su.sun_color_fog.rgb*spec;"
"col=LERP(col,float3(1.18,1.22,1.28)*(1.0-0.45*night),foam);"
"float edge_a=smoothstep(0.0,0.032,wdepth)*LERP(0.64,0.93,SAT(vdepth*0.85+foam*0.70));"
"return float4(col,SAT(edge_a));"
"}"
"struct VfxIn {"
"float3 pos   [[attribute(0)]];"
"float2 uv    [[attribute(1)]];"
"uchar4 rgba  [[attribute(2)]];"
"float2 kp    [[attribute(3)]];"
"};"
"struct VfxOut { float4 pos [[position]];float2 uv;float4 col;float2 kp;};"
"vertex VfxOut vs_vfx(VfxIn v [[stage_in]],constant SceneUniforms &su [[buffer(0)]]){"
"VfxOut o;"
"float3 cwp=apply_world_curve(v.pos,su.sky_zenith_pad,su.tile_overlay[41]);"
"o.pos=su.view_proj*float4(cwp,1.0);"
"o.uv=v.uv;"
"o.col=float4(pow(max(float3(v.rgba.rgb)/255.0,float3(0.0)),float3(2.2)),float(v.rgba.a)/255.0);"
"o.kp=v.kp;"
"return o;"
"}"
"fragment float4 fs_vfx(VfxOut in [[stage_in]],constant SceneUniforms &su [[buffer(0)]],\n"
"depth2d<float> tex_depth [[texture(0)]]){"
"uint2 pix=min(uint2(in.pos.xy),uint2(tex_depth.get_width()-1u,tex_depth.get_height()-1u));"
"float scene_z=tex_depth.read(pix);"
"float lin_s=linear_eye_z(scene_z,su);"
"float lin_v=linear_eye_z(in.pos.z,su);"
"int k=int(in.kp.x+0.5);"
"if(k==4){"
"if(lin_v > lin_s+0.045) discard_fragment();"
"float2 p=in.uv*2.0-1.0;"
"float dz=abs(lin_s-lin_v);"
"float fall=pow(SAT(1.0-dot(p,p)),1.50)*rsmooth(0.30,0.04,dz);"
"float sa=in.col.a*fall;"
"return float4(float3(0.008,0.014,0.034)*sa,sa);"
"}"
"if(in.pos.z > scene_z) discard_fragment();"
"float a=in.col.a*smoothstep(0.0,0.18,lin_s-lin_v);"
"float3 c=in.col.rgb*3.0;"
"if(k==0){"
"float blade=sin(in.uv.x*3.14159)*pow(in.uv.y,1.6);"
"float core=smoothstep(0.68,0.98,in.uv.y)*sin(in.uv.x*3.14159);"
"c=LERP(c,float3(4.2,4.1,3.9),core);"
"a *= blade;"
"} else if(k==1){"
"float2 p=in.uv*2.0-1.0;"
"float r=length(p),ang=atan2(p.y,p.x);"
"float r1=rsmooth(0.05,0.01,abs(r-0.88));"
"float r2=rsmooth(0.04,0.01,abs(r-0.64));"
"float star=rsmooth(0.05,0.01,abs(r-(0.58+0.22*cos(ang*6.0))));"
"float runes=step(0.3,sin(ang*24.0))*rsmooth(0.09,0.02,abs(r-0.76));"
"a *= SAT(r1+r2+star+runes*0.85);"
"c *= 1.25;"
"} else if(k==3){"
"float wave=in.uv.y+0.16*sin(in.uv.x*6.28318);"
"float core=pow(sin(in.uv.x*3.14159),2.0)*smoothstep(0.0,0.45,wave)*rsmooth(1.0,0.55,wave);"
"c=LERP(in.col.rgb*1.15,float3(1.45,1.48,1.52),core*0.5);"
"a *= core;"
"} else {"
"float d=length(in.uv*2.0-1.0);"
"a *= rsmooth(1.0,0.15,d);"
"c *= 0.82;"
"}"
"return float4(c*a,a);"
"}"
"fragment float4 fs_bloom_down(FsQuadOut in [[stage_in]],texture2d<float> src [[texture(0)]],\n"
"sampler smp [[sampler(0)]],constant float4 &par [[buffer(0)]]){"
"float2 off=par.xy;"
"float th=par.z;"
"float3 c =bloom_karis_tap(src.sample(smp,in.uv).rgb,th)*0.5;"
"c += bloom_karis_tap(src.sample(smp,in.uv+float2(-off.x,-off.y)).rgb,th)*0.125;"
"c += bloom_karis_tap(src.sample(smp,in.uv+float2( off.x,-off.y)).rgb,th)*0.125;"
"c += bloom_karis_tap(src.sample(smp,in.uv+float2(-off.x, off.y)).rgb,th)*0.125;"
"c += bloom_karis_tap(src.sample(smp,in.uv+float2( off.x, off.y)).rgb,th)*0.125;"
"return float4(c,1.0);"
"}"
"fragment float4 fs_composite(FsQuadOut in [[stage_in]],constant SceneUniforms &su [[buffer(0)]],\n"
"texture2d<float> hdr_tex   [[texture(0)]],\n"
"texture2d<float> bloom1    [[texture(1)]],\n"
"texture2d<float> bloom2    [[texture(2)]],\n"
"texture2d<float> bloom3    [[texture(3)]],\n"
"depth2d<float>   tex_depth [[texture(4)]],\n"
"sampler smp [[sampler(0)]]){"
"int2 dim=int2(tex_depth.get_width(),tex_depth.get_height());"
"int2 p0 =clamp(int2(in.pos.xy),int2(0),dim-1);"
"float z0=tex_depth.read(uint2(p0));"
"float d0=linear_eye_z(z0,su);"
"float coc=0.0;"
"if(z0 < 0.9998){"
"float foc_d=max(su.tile_overlay[41].z,2.5);"
"coc=LERP(smoothstep(foc_d+13.5,foc_d+28.0,d0)*0.52,smoothstep(foc_d+2.4,foc_d+10.5,d0)*0.85,su.tile_overlay[41].w);"
"}"
"float3 hdr=hdr_tex.sample(smp,in.uv).rgb;"
"if(coc > 0.02){"
"float3 acc=hdr;"
"float wsum=1.0;"
"float ang0=ign21(in.pos.xy)*6.283185;"
"float2 b_rad=su.screen_params.zw*(coc*4.8);"
"for(int i=0;i < 6;i++){"
"float a=ang0+float(i)*2.399963;"
"float r=sqrt((float(i)+0.5)/6.0);"
"float2 off=float2(cos(a),sin(a))*r*b_rad;"
"int2 tp=clamp(p0+int2(off*su.screen_params.xy),int2(0),dim-1);"
"float td=linear_eye_z(tex_depth.read(uint2(tp)),su);"
"float w=smoothstep(-1.2,0.2,td-d0);"
"acc += hdr_tex.sample(smp,clamp(in.uv+off,0.001,0.999)).rgb*w;"
"wsum += w;"
"}"
"hdr=acc/wsum;"
"}"
"if(z0 < 0.9995&&d0 < 32.0){"
"float ang=ign21(in.pos.xy+float2(17.0,31.0))*3.14159;"
"float occ=0.0;"
"for(int i=0;i < 4;i++){"
"float a=ang+float(i)*0.785398;"
"float px_r=((i & 1)==0?5.5:13.5)*clamp(10.0/max(d0,2.5),0.55,2.2);"
"int2 step_p=int2(round(float2(cos(a),sin(a))*px_r));"
"float dp=linear_eye_z(tex_depth.read(uint2(clamp(p0+step_p,int2(0),dim-1))),su);"
"float dn=linear_eye_z(tex_depth.read(uint2(clamp(p0-step_p,int2(0),dim-1))),su);"
"float valley=d0-0.5*(dp+dn);"
"float rng_w=rsmooth(0.95,0.16,max(abs(d0-dp),abs(d0-dn)));"
"occ += SAT((valley-0.014)*3.8)*rng_w;"
"}"
"float ao=1.0-SAT(occ*0.28)*rsmooth(32.0,18.0,d0)*0.34;"
"hdr *= LERP(float3(0.86,0.90,0.95)*ao,float3(1.0),ao);"
"}"
"float3 bl=bloom1.sample(smp,in.uv).rgb*0.42+bloom2.sample(smp,in.uv).rgb*0.58+bloom3.sample(smp,in.uv).rgb*0.72;"
"float2 sun_uv=float2(su.sun_screen_pos.x,1.0-su.sun_screen_pos.y);"
"float sun_w=su.sun_screen_pos.z;"
"if(sun_w > 0.01){"
"float2 delta=(sun_uv-in.uv)*(0.72/12.0);"
"float2 ruv=in.uv;"
"float3 rays=float3(0.0);"
"float dec=1.0;"
"for(int i=0;i < 12;i++){"
"ruv += delta;"
"rays += bloom1.sample(smp,clamp(ruv,0.002,0.998)).rgb*dec;"
"dec *= 0.88;"
"}"
"hdr += rays*float3(1.0,0.82,0.52)*(0.042*sun_w);"
"}"
"hdr += bl*su.sun_screen_pos.w;"
"float3 ldr=agx_tonemap(hdr);"
"float2 vd=in.uv-0.5;"
"ldr *= 1.0-0.18*dot(vd,vd);"
"return float4(ldr,1.0);"
"}"
"struct UiPrim {"
"float4 rect;"
"float4 uv_rect;"
"uint col0;"
"uint col1;"
"float radius;"
"float border;"
"uint kind;"
"uint glyph_bits;"
"uint2 pad;"
"};"
"struct UiOut {"
"float4 pos [[position]];"
"float2 px;"
"float2 uv;"
"float4 rect;"
"float4 c0;"
"float4 c1;"
"float2 rb;"
"uint kind;"
"uint bits;"
"};"
"float4 unpack_rgba(uint c){"
"return float4(float(c & 255u),float((c >> 8) & 255u),float((c >> 16) & 255u),float((c >> 24) & 255u))/255.0;"
"}"
"vertex UiOut vs_ui(uint vid [[vertex_id]],uint iid [[instance_id]],\n"
"constant SceneUniforms &su [[buffer(0)]],\n"
"constant UiPrim *prims [[buffer(1)]]){"
"UiOut o;"
"UiPrim p=prims[iid];"
"float2 c=float2((vid==1||vid==2||vid==4)?1.0:0.0,(vid>=2&&vid!=3)?1.0:0.0);"
"float2 pad=(p.kind==1u)?float2(2.0):float2(0.0);"
"float2 px=LERP(p.rect.xy-pad,p.rect.zw+pad,c);"
"o.px=px;"
"o.uv=LERP(p.uv_rect.xy,p.uv_rect.zw,c);"
"o.rect=p.rect;"
"o.c0=unpack_rgba(p.col0);"
"o.c1=unpack_rgba(p.col1);"
"o.rb=float2(p.radius,p.border);"
"o.kind=p.kind;"
"o.bits=p.glyph_bits;"
"float2 ndc=float2((px.x*su.screen_params.z)*2.0-1.0,1.0-(px.y*su.screen_params.w)*2.0);"
"o.pos=float4(ndc,0.0,1.0);"
"return o;"
"}"
"float glyph_sdf(float2 uv,uint bits){"
"float2 gp=uv*float2(5.0,6.0)-0.5;"
"float md=4.0;"
"for(int r=0;r < 6;r++){"
"for(int c=0;c < 5;c++){"
"uint idx=uint(r*5+c);"
"if(((bits >> idx) & 1u)!=0u){"
"float2 center=float2(float(c),float(r));"
"md=min(md,length(gp-center));"
"if(c < 4&&((bits >> (idx+1u)) & 1u)!=0u){"
"float2 pa=gp-center,ba=float2(1.0,0.0);"
"md=min(md,length(pa-ba*SAT(dot(pa,ba))));"
"}"
"if(r < 5&&((bits >> (idx+5u)) & 1u)!=0u){"
"float2 pa=gp-center,ba=float2(0.0,1.0);"
"md=min(md,length(pa-ba*SAT(dot(pa,ba))));"
"}"
"if(c < 4&&r < 5&&((bits >> (idx+6u)) & 1u)!=0u){"
"float2 pa=gp-center,ba=float2(1.0,1.0);"
"md=min(md,length(pa-ba*SAT(dot(pa,ba)*0.5)));"
"}"
"if(c > 0&&r < 5&&((bits >> (idx+4u)) & 1u)!=0u){"
"float2 pa=gp-center,ba=float2(-1.0,1.0);"
"md=min(md,length(pa-ba*SAT(dot(pa,ba)*0.5)));"
"}"
"}"
"}"
"}"
"return md;"
"}"
"fragment float4 fs_ui(UiOut in [[stage_in]]){"
"if(in.kind==1){"
"float2 sz=max(in.rect.zw-in.rect.xy,float2(1.0));"
"float2 uv=(in.px-in.rect.xy)/sz;"
"float d_fg=glyph_sdf(uv,in.bits);"
"float d_sh=glyph_sdf(uv-float2(0.06,0.07),in.bits);"
"float a_fg=smoothstep(0.56,0.34,d_fg);"
"float a_sh=smoothstep(0.68,0.36,d_sh)*in.c1.a;"
"float3 rgb=LERP(in.c1.rgb,in.c0.rgb,a_fg);"
"float a=SAT(max(a_sh,a_fg*in.c0.a));"
"return float4(rgb,a);"
"}"
"float2 center=0.5*(in.rect.xy+in.rect.zw);"
"float2 half_sz=0.5*(in.rect.zw-in.rect.xy);"
"float rad=min(in.rb.x,min(half_sz.x,half_sz.y));"
"float2 q=abs(in.px-center)-(half_sz-rad);"
"float sdf=length(max(q,0.0))+min(max(q.x,q.y),0.0)-rad;"
"float alpha=smoothstep(0.8,-0.8,sdf);"
"if(alpha<=0.002) discard_fragment();"
"float4 col=in.c0;"
"float gy=SAT((in.px.y-in.rect.y)/max(1.0,in.rect.w-in.rect.y));"
"col.rgb *= (1.14-0.24*gy);"
"if(in.rb.y > 0.1){"
"float b_mask=smoothstep(-in.rb.y-0.8,-in.rb.y+0.6,sdf);"
"col=LERP(col,in.c1,b_mask);"
"}"
"col.a *= alpha;"
"return col;"
"}";
#endif // __APPLE__

// ============================================================================
// ANDROID OPENGL ES 3.00 (GLSL ES 300) COMPLETE SOURCES
// ============================================================================
#ifdef __ANDROID__
static const char *k_glsl_preamble =
"#version 300 es\n"
"precision highp float;"
"precision highp int;"
"precision highp sampler2DShadow;\n"
"#define F2 vec2\n"
"#define F3 vec3\n"
"#define F4 vec4\n"
"#define LERP mix\n"
"#define SAT(x) clamp((x), 0.0, 1.0)\n"
"#define FRAC fract\n"
"#define OUT_F3(x) out vec3 x\n"
"#define OUT_F1(x) out float x\n"
SH_SHARED_CORE
"layout(std140) uniform SceneBlock {"
"mat4 view;"
"mat4 proj;"
"mat4 view_proj;"
"mat4 inv_view_proj;"
"mat4 light_view_proj;"
"vec4 cam_pos_time;"
"vec4 sun_dir_intensity;"
"vec4 sun_color_fog;"
"vec4 sky_zenith_pad;"
"vec4 sky_horizon_pad;"
"vec4 grid_params;"
"vec4 hover_cursor;"
"vec4 screen_params;"
"vec4 sun_screen_pos;"
"vec4 tile_overlay[42];"
"} su;"
"layout(std140) uniform DrawBlock {"
"mat4 model;"
"vec4 tint_outline;"
"vec4 hair_color;"
"vec4 custom_params;"
"mat4 bones[16];"
"} du;"
"float linear_eye_z(float z_buf){ return su.proj[3][2]/(z_buf*2.0-1.0+su.proj[2][2]);}";

static const char *k_gles_vs_fullscreen =
"#version 300 es\n"
"out vec2 v_uv;"
"void main(){"
"vec2 uv=vec2((gl_VertexID << 1) & 2,gl_VertexID & 2);"
"v_uv=uv;"
"gl_Position=vec4(uv*2.0-1.0,0.0,1.0);"
"}";

static const char *k_gles_fs_gen_tex =
"in vec2 v_uv;"
"uniform int u_kind;"
"out vec4 o_col;"
"void main(){ o_col=gen_proc_tex(v_uv,u_kind);}";

static const char *k_gles_fs_sky =
"in vec2 v_uv;"
"out vec4 o_col;"
"void main(){"
"vec2 ndc=v_uv*2.0-1.0;"
"vec4 wp=su.inv_view_proj*vec4(ndc,1.0,1.0);"
"vec3 dir=wp.xyz/wp.w-su.cam_pos_time.xyz;"
"o_col=vec4(eval_sky_dome(dir,su.sun_dir_intensity.xyz,su.cam_pos_time.w,su.sky_zenith_pad.rgb,su.sky_horizon_pad.rgb,su.sky_horizon_pad.w),1.0);"
"}";

#define GLSL_SKIN_HELPER \
"layout(location=0) in vec3 a_pos;" \
"layout(location=1) in vec3 a_nrm;" \
"layout(location=2) in vec2 a_uv;" \
"layout(location=3) in uvec4 a_col;" \
"layout(location=4) in uvec4 a_par;" \
"layout(location=5) in uvec4 a_jnt;" \
"layout(location=6) in uvec4 a_wgt;" \
"vec4 skin_pos(out vec3 out_n){" \
"vec3 p=a_pos,n=a_nrm;" \
"if(du.custom_params.w > 0.5){" \
"vec4 w=vec4(a_wgt)/255.0;" \
"mat4 skin=du.bones[a_jnt.x]*w.x+du.bones[a_jnt.y]*w.y;" \
"p=(skin*vec4(p,1.0)).xyz;" \
"n=normalize((skin*vec4(n,0.0)).xyz);" \
"}" \
"out_n=normalize((du.model*vec4(n,0.0)).xyz);" \
"return du.model*vec4(p,1.0);" \
"}"

static const char *k_gles_vs_shadow =
GLSL_SKIN_HELPER
"void main(){"
"vec3 wn;vec4 wp=skin_pos(wn);"
"if(a_par.x==2u){ wp.x += sin(su.cam_pos_time.w*2.1+wp.z*0.7+wp.y*0.5)*0.065;}"
"gl_Position=su.light_view_proj*wp;"
"}";

static const char *k_gles_fs_empty = "#version 300 es\nvoid main(){}\n";

static const char *k_gles_vs_outline =
GLSL_SKIN_HELPER
"out vec4 v_ocol;"
"void main(){"
"vec3 wn;vec4 wp=skin_pos(wn);"
"float ow=(float(a_par.w)/255.0)*du.tint_outline.w;"
"vec3 cwp=apply_world_curve(wp.xyz,su.sky_zenith_pad,su.tile_overlay[41]);"
"vec4 cp=su.view_proj*vec4(cwp,1.0);"
"if(ow<=0.001){ gl_Position=vec4(0.0,0.0,-2.0,1.0);v_ocol=vec4(0.0);return;}"
"vec3 cn=(su.view_proj*vec4(wn,0.0)).xyz;"
"vec2 dir2=normalize(cn.xy+vec2(1e-5));"
"float px_scale=clamp(cp.w,1.2,20.0);"
"cp.xy += dir2*ow*2.15*su.screen_params.zw*px_scale;"
"cp.z += 0.0040*cp.w;"
"gl_Position=cp;"
"v_ocol=vec4(0.11,0.072,0.085,1.0);"
"}";

static const char *k_gles_fs_outline =
"#version 300 es\nprecision highp float;\nin vec4 v_ocol;\nout vec4 o_col;\n"
"void main(){ o_col=v_ocol;}";

static const char *k_gles_vs_main =
GLSL_SKIN_HELPER
"out vec3 v_wpos;\nout vec3 v_wnrm;\nout vec2 v_uv;\nout vec4 v_vcol;\nout vec2 v_rm;\nflat out int v_mat;"
"void main(){"
"vec3 wn;vec4 wp=skin_pos(wn);"
"if(a_par.x==2u){ wp.x += sin(su.cam_pos_time.w*2.1+wp.z*0.7+wp.y*0.5)*0.068;}"
"v_wpos=wp.xyz;v_wnrm=wn;v_uv=a_uv;"
"v_vcol=vec4(pow(max(vec3(a_col.rgb)/255.0,vec3(0.0)),vec3(2.2)),float(a_col.a)/255.0);"
"v_rm=vec2(float(a_par.y)/255.0,float(a_par.z)/255.0);"
"v_mat=int(a_par.x);"
"vec3 cwp=apply_world_curve(wp.xyz,su.sky_zenith_pad,su.tile_overlay[41]);"
"gl_Position=su.view_proj*vec4(cwp,1.0);"
"}";

static const char *k_gles_fs_main =
"in vec3 v_wpos;\nin vec3 v_wnrm;\nin vec2 v_uv;\nin vec4 v_vcol;\nin vec2 v_rm;\nflat in int v_mat;"
"uniform sampler2D tex_marble;"
"uniform sampler2D tex_grass;"
"uniform sampler2D tex_fabric;"
"uniform sampler2DShadow tex_shadow;"
"out vec4 o_col;"
"float sample_pcf_shadow(vec3 wpos,vec3 N){"
"vec4 lp=su.light_view_proj*vec4(wpos+N*0.022,1.0);"
"vec3 ndc=lp.xyz/lp.w*0.5+0.5;"
"if(ndc.x < 0.01||ndc.x > 0.99||ndc.y < 0.01||ndc.y > 0.99||ndc.z < 0.0||ndc.z > 1.0) return 1.0;"
"float z_ref=ndc.z-0.0018;"
"float ang=ign21(gl_FragCoord.xy)*6.283185;"
"float sum=0.0,rad=2.05/2048.0;"
"for(int i=0;i<8;i++){"
"float r=sqrt((float(i)+0.5)*0.125)*rad,th=float(i)*2.399963+ang;"
"sum += texture(tex_shadow,vec3(ndc.xy+vec2(cos(th),sin(th))*r,z_ref));"
"}"
"return sum*0.125;"
"}"
"void main(){"
"int mat=v_mat;"
"float rough=v_rm.x,metal=v_rm.y;"
"vec3 N=normalize(v_wnrm),V=normalize(su.cam_pos_time.xyz-v_wpos),L=normalize(su.sun_dir_intensity.xyz),H=normalize(L+V);"
"vec3 albedo=v_vcol.rgb;"
"bool is_char=(du.custom_params.w > 0.5);"
"if(mat==0){"
"vec2 muv=v_wpos.xz*0.22;"
"vec4 m=texture(tex_marble,muv);"
"float mx=texture(tex_marble,muv+vec2(0.004,0.0)).a,mz=texture(tex_marble,muv+vec2(0.0,0.004)).a;"
"N=normalize(N+vec3(m.a-mx,0.0,m.a-mz)*1.15);"
"albedo *= mix(vec3(0.85),m.rgb*1.12,0.85);rough=mix(0.46,0.22,m.a);"
"} else if(mat==1){"
"vec4 gr=texture(tex_grass,v_wpos.xz*0.18);"
"vec2 ruv=vec2(v_wpos.x*0.11+v_wpos.z*0.11,v_wpos.y*0.16);"
"vec4 rk=texture(tex_grass,ruv);"
"float rk_y=texture(tex_grass,ruv+vec2(0.0,0.006)).a;"
"float gm=smoothstep(0.66,0.86,N.y)*smoothstep(-0.05,0.24,v_wpos.y);"
"vec3 cliff_c=mix(vec3(0.32,0.22,0.15),vec3(0.64,0.50,0.36),rk.a);"
"vec3 cliff_N=normalize(N+vec3(0.0,(rk.a-rk_y)*1.4,0.0));"
"vec3 turf_c=gr.rgb*1.18;"
"vec2 tc=fract(v_wpos.xz*0.55)-0.5;"
"float tri=step(abs(tc.x)*1.6+tc.y,0.18)*step(-0.22,tc.y);"
"turf_c=mix(turf_c*vec3(0.88,1.06,0.82),turf_c*vec3(1.06,1.15,0.84),tri*0.34);"
"albedo=mix(cliff_c,turf_c,gm);N=normalize(mix(cliff_N,N,gm));"
"float north_plat=rsmooth(-3.0,-3.5,v_wpos.z)*rsmooth(8.8,8.2,abs(v_wpos.x));"
"float sand_m=rsmooth(0.40,0.24,v_wpos.y)*smoothstep(0.32,0.68,N.y)*(1.0-north_plat);"
"if(sand_m > 0.005){"
"float grain=vnoise(v_wpos.xz*6.5)*0.5+vnoise(v_wpos.xz*22.0)*0.5;"
"float ripple=0.5+0.5*sin(v_wpos.x*2.4+v_wpos.z*1.8+grain*1.6);"
"float wet_lip=rsmooth(0.30,0.06,v_wpos.y)*smoothstep(-0.08,0.05,v_wpos.y);"
"float glit=smoothstep(0.955,0.995,hash21(floor(v_wpos.xz*52.0)))*(1.0-wet_lip*0.4);"
"vec3 sand_c=vec3(0.93,0.79,0.54)*(0.93+0.13*ripple)+vec3(0.45,0.42,0.35)*glit;"
"sand_c=mix(sand_c,sand_c*vec3(0.62,0.56,0.50),wet_lip*0.72);"
"albedo=mix(albedo,sand_c,sand_m);rough=mix(rough,mix(0.76,0.16,wet_lip),sand_m);"
"N=normalize(N+vec3(cos(v_wpos.x*2.4+v_wpos.z*1.8)*0.08*sand_m,0.0,0.0));"
"}"
"float d_pav=min(sd_rbox2(v_wpos.xz-vec2(-1.60,1.80),vec2(2.62,2.62),0.52),sd_rbox2(v_wpos.xz-vec2(-5.52,-1.60),vec2(1.24,2.04),0.24));"
"float pav_m=rsmooth(0.06,-0.04,d_pav)*smoothstep(0.38,0.52,v_wpos.y)*smoothstep(0.55,0.75,N.y);"
"if(pav_m > 0.005){"
"vec2 puv=v_wpos.xz*0.28;"
"vec4 pm=texture(tex_marble,puv);"
"float px=texture(tex_marble,puv+vec2(0.005,0.0)).a,pz=texture(tex_marble,puv+vec2(0.0,0.005)).a;"
"float curb=smoothstep(-0.14,-0.05,d_pav)*rsmooth(0.06,-0.02,d_pav);"
"vec3 stone_c=mix(pm.rgb*vec3(0.96,0.89,0.77)*(0.92+0.14*pm.a),vec3(0.84,0.73,0.58),curb*0.75);"
"albedo=mix(albedo,stone_c,pav_m);rough=mix(rough,mix(0.54,0.30,pm.a),pav_m);"
"N=normalize(N+vec3(pm.a-px,0.0,pm.a-pz)*(1.25*pav_m));"
"}"
"} else if(mat==2||mat==3){"
"float cam_d=length(su.cam_pos_time.xyz-v_wpos);"
"if(cam_d < 2.4){"
"float fade=smoothstep(1.2,2.4,cam_d);"
"float bayer=fract(dot(floor(gl_FragCoord.xy),vec2(0.5,0.25))+fract(gl_FragCoord.x*0.125)*0.5);"
"if(fade < bayer) discard;"
"}"
"if(mat==2){"
"vec4 lf=texture(tex_grass,v_wpos.xz*0.48+v_wpos.y*0.30);"
"float cl_p=1.0-voronoi2(v_wpos.xz*2.6+v_wpos.y*1.8);"
"albedo *= (0.84+0.30*lf.g)*(0.90+0.18*cl_p);"
"N=normalize(N+vec3(lf.r-0.35,(cl_p-0.45)*0.35,lf.g-0.45)*0.22);"
"} else {"
"float bark=sin(v_uv.x*37.699+fbm4(v_wpos.xy*3.5)*3.2);"
"albedo *= (0.92+0.12*bark);N=normalize(N+vec3(bark*0.12,0.0,-bark*0.08));"
"}"
"} else if(mat==4){ albedo=eval_villager_face(v_uv,albedo,du.custom_params.y);}"
"else if(mat==5){ albedo=pow(max(du.hair_color.rgb,vec3(0.001)),vec3(2.2))*(0.74+0.34*v_uv.y);}"
"else if(mat==6){"
"if(v_uv.x>=1.95) albedo *= pow(max(du.tint_outline.rgb,vec3(0.001)),vec3(2.2));"
"if(v_uv.x>=4.95) albedo=eval_aloha_shirt(v_uv,albedo);"
"albedo *= (0.90+0.16*texture(tex_fabric,v_uv*2.5).g);"
"}"
"else if(mat==8){"
"if(v_vcol.a < 0.94){"
"float rim_f=pow(1.0-SAT(abs(dot(N,V))),2.2);"
"float hex_g=rsmooth(0.14,0.03,abs(voronoi2(v_wpos.xy*4.5+v_wpos.z*3.2+su.cam_pos_time.w*0.6)-0.34));"
"if(SAT(rim_f*0.88+hex_g*0.46)*v_vcol.a*1.35 < ign21(gl_FragCoord.xy)) discard;"
"o_col=vec4((albedo*du.tint_outline.rgb)*(1.65+1.85*rim_f+0.95*hex_g),1.0);return;"
"}"
"o_col=vec4(albedo*(2.4+1.5*du.custom_params.z),1.0);return;"
"}"
"else if(mat==9){"
"float grain=vnoise(v_wpos.xz*6.5)*0.5+vnoise(v_wpos.xz*22.0)*0.5;"
"float ripple=0.5+0.5*sin(v_wpos.x*2.4+v_wpos.z*1.8+grain*1.6);"
"float wet=smoothstep(0.30,-0.06,v_wpos.y);"
"albedo *= (0.92+0.14*ripple);"
"albedo=mix(albedo,albedo*vec3(0.58,0.52,0.48),wet*0.78);"
"rough=mix(0.76,0.16,wet);"
"} else if(mat==10){"
"if(v_uv.x>=7.5){ vec3 rn;float rr;albedo=eval_roof_shingles(v_uv-vec2(8.0,0.0),albedo,N,rn,rr);N=rn;rough=rr;}"
"else { albedo *= (0.94+0.08*sin(v_uv.y*25.1327));}"
"}"
"float shadow=sample_pcf_shadow(v_wpos,N)*(0.80+0.20*smoothstep(0.35,0.72,texture(tex_fabric,v_wpos.xz*0.025+su.cam_pos_time.w*0.012).b));"
"float ndl=dot(N,L);"
"vec3 sun_col=su.sun_color_fog.rgb*su.sun_dir_intensity.w;"
"vec3 bounce_c=vec3(0.24,0.36,0.24)*SAT(-N.y*0.55+0.45)*su.sun_dir_intensity.w*0.13;"
"vec3 lit=vec3(0.0);"
"if(is_char){"
"float cel=smoothstep(-0.08,0.14,ndl*(0.38+0.62*shadow));"
"vec3 sh_tint=albedo*(mix(vec3(0.42,0.40,0.62),su.sky_zenith_pad.rgb*0.55+vec3(0.24,0.26,0.38),0.68)+bounce_c);"
"vec3 diff=mix(sh_tint,albedo*vec3(1.06,1.00,0.94),cel);"
"if(mat==4) diff += albedo*vec3(0.18,0.05,0.04)*smoothstep(-0.2,0.25,ndl)*(1.0-cel);"
"float sp_r=pow(max(SAT(dot(N,H)),1e-4),exp2(mix(4.0,9.0,1.0-rough)));"
"float spec=sp_r*(metal > 0.4?1.65:0.16)*cel;"
"vec3 spec_col=mix(vec3(1.0),albedo*1.6+vec3(0.25),metal)*spec*sun_col*0.40;"
"if(mat==5){"
"float r1=smoothstep(0.82,0.95,SAT(1.0-abs(N.y-0.50)))*cel;"
"spec_col += (albedo+vec3(0.28,0.20,0.14))*r1*0.24+su.sky_horizon_pad.rgb*smoothstep(0.86,0.97,SAT(1.0-abs(N.y-0.28)))*0.08;"
"}"
"float rim=pow(1.0-SAT(dot(N,V)),3.5)*SAT(dot(V,-L)*0.6+0.55);"
"lit=mix(diff*1.15+spec_col+sun_col*(rim*0.26),vec3(3.5,1.4,1.2),du.custom_params.x);"
"} else {"
"float wrap_d=SAT((ndl+0.25)/1.25)*shadow;"
"vec3 amb_top=su.sky_zenith_pad.rgb*0.46+vec3(0.15,0.19,0.25);"
"vec3 amb_bot=vec3(0.11,0.13,0.18)+bounce_c;"
"vec3 amb=mix(amb_bot,amb_top,N.y*0.5+0.5);"
"float spec=pow(max(SAT(dot(N,H)),1e-4),mix(8.0,84.0,1.0-rough))*(1.0-rough)*shadow*0.46;"
"float sss=(mat==2)?(pow(SAT(dot(V,-L)),2.2)*0.52+0.22*SAT(N.y))*(0.45+0.55*shadow):0.0;"
"lit=albedo*(amb+sun_col*wrap_d*0.58+vec3(0.95,0.88,0.38)*sss)+sun_col*spec;"
"}"
"for(int li=0;li < 8;li++){"
"vec4 pl=su.tile_overlay[li];"
"if(pl.w > 0.05){ vec3 lv=pl.xyz-v_wpos;float att=pl.w/(1.0+dot(lv,lv)*0.28);lit += (albedo*0.65+vec3(0.35))*vec3(1.25,0.78,0.34)*att*(0.45+0.55*SAT(dot(N,normalize(lv))));}"
"}"
"if(su.hover_cursor.z > 0.5&&!is_char&&N.y > 0.65){"
"float hr=length(v_wpos.xz-su.hover_cursor.xy);"
"lit += vec3(1.35,1.15,0.45)*rsmooth(0.68,0.58,hr)*smoothstep(0.42,0.52,hr)*(0.75+0.25*sin(su.cam_pos_time.w*5.0));"
"}"
"float fog=1.0-exp(-max(0.0,length(su.cam_pos_time.xyz-v_wpos)-15.0)*su.sun_color_fog.w);"
"o_col=vec4(mix(lit,su.tile_overlay[40].rgb,SAT(fog*0.68)),v_vcol.a);"
"}";

static const char *k_gles_vs_grass =
"layout(location=0) in vec3 a_pos;\nlayout(location=2) in vec2 a_uv;"
"layout(location=7) in vec4 i_pos_scale;\nlayout(location=8) in vec4 i_rot_col;"
"out vec3 v_wpos;\nout vec3 v_col;\nout float v_gy;"
"void main(){"
"float ca=cos(i_rot_col.x),sa=sin(i_rot_col.x);"
"vec3 lp=a_pos*i_pos_scale.w;"
"vec3 rp=vec3(lp.x*ca-lp.z*sa,lp.y,lp.x*sa+lp.z*ca);"
"float gust=sin(su.cam_pos_time.w*2.6+i_pos_scale.x*0.35+i_pos_scale.z*0.25+i_rot_col.y);"
"float bend=a_uv.y*a_uv.y*(0.14+0.10*gust);"
"rp.x += bend;rp.y -= bend*0.25;"
"v_wpos=i_pos_scale.xyz+rp;v_gy=a_uv.y;"
"v_col=mix(vec3(0.11,0.36,0.08),vec3(0.46,0.80,0.19),a_uv.y)*i_rot_col.w;"
"if(i_rot_col.z > 0.5&&a_uv.y > 0.68){"
"if(i_rot_col.z > 2.5) v_col=vec3(1.35,0.42,0.62);"
"else if(i_rot_col.z > 1.5) v_col=vec3(0.35,0.78,1.35);"
"else v_col=vec3(1.35,0.95,0.25);"
"}"
"vec3 cwp=apply_world_curve(v_wpos,su.sky_zenith_pad,su.tile_overlay[41]);"
"gl_Position=su.view_proj*vec4(cwp,1.0);"
"}";

static const char *k_gles_fs_grass =
"in vec3 v_wpos;\nin vec3 v_col;\nin float v_gy;"
"uniform sampler2DShadow tex_shadow;"
"out vec4 o_col;"
"void main(){"
"vec4 lp=su.light_view_proj*vec4(v_wpos+vec3(0.0,0.02,0.0),1.0);"
"vec3 ndc=lp.xyz/lp.w*0.5+0.5;"
"float sh=texture(tex_shadow,vec3(ndc.xy,ndc.z-0.0018));"
"vec3 amb=su.sky_zenith_pad.rgb*0.40+vec3(0.09,0.11,0.16);"
"vec3 lit=v_col*mix(0.68,1.12,v_gy)*(amb+su.sun_color_fog.rgb*su.sun_dir_intensity.w*(0.20+0.54*sh));"
"for(int li=0;li<8;li++){ vec4 pl=su.tile_overlay[li];if(pl.w > 0.05){ vec3 lv=pl.xyz-v_wpos;lit += v_col*vec3(1.2,0.75,0.32)*(pl.w/(1.0+dot(lv,lv)*0.30));} }"
"o_col=vec4(lit,1.0);"
"}";

static const char *k_gles_vs_water =
"layout(location=0) in vec3 a_pos;\nlayout(location=1) in vec3 a_nrm;\nlayout(location=2) in vec2 a_uv;\nlayout(location=3) in uvec4 a_col;"
"out vec3 v_wpos;\nout vec3 v_wnrm;\nout vec2 v_uv;\nout vec4 v_col;"
"void main(){"
"vec3 wp=a_pos;"
"if(a_uv.x < 9.0) wp.y += sin(su.cam_pos_time.w*2.3+wp.x*1.2)*cos(su.cam_pos_time.w*1.7+wp.z*1.4)*0.032;"
"v_wpos=wp;v_wnrm=a_nrm;v_uv=a_uv;"
"v_col=vec4(vec3(a_col.rgb)/255.0,float(a_col.a)/255.0);"
"vec3 cwp=apply_world_curve(wp,su.sky_zenith_pad,su.tile_overlay[41]);"
"gl_Position=su.view_proj*vec4(cwp,1.0);"
"}";

static const char *k_gles_fs_water =
"in vec3 v_wpos;\nin vec3 v_wnrm;\nin vec2 v_uv;\nin vec4 v_col;"
"uniform sampler2D tex_fabric;"
"uniform sampler2D tex_depth;"
"out vec4 o_col;"
"void main(){"
"ivec2 dim=textureSize(tex_depth,0);"
"ivec2 pix=clamp(ivec2(gl_FragCoord.xy),ivec2(0),dim-1);"
"float scene_z=texelFetch(tex_depth,pix,0).r;"
"if(gl_FragCoord.z > scene_z) discard;"
"float wdepth=max(0.0,linear_eye_z(scene_z)-linear_eye_z(gl_FragCoord.z));"
"float t=su.cam_pos_time.w;"
"bool is_fall=(v_uv.x>=9.5);"
"vec3 V=normalize(su.cam_pos_time.xyz-v_wpos);"
"float vdepth=wdepth*max(0.28,abs(V.y));"
"vec2 uv1=is_fall?vec2(v_uv.x*2.0,v_uv.y-t*1.8):(v_wpos.xz*0.22+vec2(t*0.075,t*0.028));"
"vec2 uv2=is_fall?vec2(v_uv.x*3.5,v_uv.y*1.4-t*2.4):(v_wpos.xz*0.33-vec2(t*0.055,-t*0.038));"
"float c1=texture(tex_fabric,uv1).r,c2=texture(tex_fabric,uv2).r,caust=(c1+c2)*0.65;"
"float rip_foam=0.0;vec2 rip_n=vec2(0.0);"
"if(!is_fall){"
"for(int ri=8;ri<=9;ri++){ vec4 rp=su.tile_overlay[ri];if(rp.w > 0.05){ vec2 dv=v_wpos.xz-rp.xz;float rd=length(dv);"
"if(rd < 2.4){ float rw=sin(rd*15.0-t*5.2)*exp(-rd*1.35)*rp.w;rip_foam += max(0.0,rw)*0.45;rip_n += (dv/max(rd,0.05))*rw*0.16;} } }"
"}"
"vec3 N=normalize(v_wnrm+vec3((c1-c2)*0.24+rip_n.x,0.0,(c2-0.4)*0.24+rip_n.y)),L=normalize(su.sun_dir_intensity.xyz);"
"vec3 sky_r=eval_sky_dome(reflect(-V,N),L,t,su.sky_zenith_pad.rgb,su.sky_horizon_pad.rgb,su.sky_horizon_pad.w);"
"float fres=0.06+0.58*pow(1.0-SAT(dot(N,V)),4.4);"
"float night=SAT(su.sky_horizon_pad.w-2.0);"
"if(is_fall){"
"vec3 fall_c=mix(vec3(0.05,0.64,0.88),vec3(0.24,0.88,0.96),SAT(caust))*(1.0-0.58*night);"
"float f_vein=smoothstep(0.38,0.72,caust)*0.80+smoothstep(0.60,0.86,c1)*0.45;"
"float f_ends=rsmooth(0.36,0.02,v_uv.y)*0.72+smoothstep(1.65,2.15,v_uv.y)*0.88+rsmooth(0.55,0.0,abs(v_wpos.y+0.1))*0.55;"
"o_col=vec4(mix(fall_c,vec3(1.20,1.25,1.32)*(1.0-0.42*night),SAT(f_vein+f_ends)),0.93);return;"
"}"
"vec3 body=mix(vec3(0.08,0.84,0.78),vec3(0.02,0.56,0.80),smoothstep(0.04,0.58,vdepth));"
"body=mix(body,vec3(0.01,0.24,0.54),smoothstep(0.50,2.50,vdepth))*(1.0-0.62*night);"
"float caust_vis=exp(-vdepth*0.65)*(0.85-0.45*night);"
"float caust_net=smoothstep(0.34,0.82,caust);"
"float spec=pow(SAT(dot(N,normalize(L+V))),220.0)*4.5*su.sun_dir_intensity.w*0.30;"
"float contact_f=rsmooth(0.145,0.012,vdepth)*(0.62+0.38*sin(vdepth*34.0-t*3.2+caust*2.5));"
"float sh=v_col.r;float w1=sin(sh*18.0-t*2.6)*0.5+0.5;"
"float surf=smoothstep(0.42,0.94,sh)*(0.32+0.56*smoothstep(0.64,0.88,w1))*rsmooth(0.85,0.08,vdepth);"
"float foam=SAT(contact_f*0.92+surf+smoothstep(0.66,0.78,caust)*0.42+rip_foam);"
"vec3 col=mix(mix(body+vec3(0.42,0.96,0.92)*caust_net*caust_vis,sky_r,fres)+su.sun_color_fog.rgb*spec,vec3(1.18,1.22,1.28)*(1.0-0.45*night),foam);"
"float edge_a=smoothstep(0.0,0.032,wdepth)*mix(0.64,0.93,SAT(vdepth*0.85+foam*0.70));"
"o_col=vec4(col,SAT(edge_a));"
"}";

static const char *k_gles_vs_vfx =
"layout(location=0) in vec3 a_pos;\nlayout(location=1) in vec2 a_uv;\nlayout(location=2) in uvec4 a_rgba;\nlayout(location=3) in vec2 a_kp;"
"out vec2 v_uv;\nout vec4 v_col;\nout vec2 v_kp;"
"void main(){"
"v_uv=a_uv;v_kp=a_kp;"
"v_col=vec4(pow(max(vec3(a_rgba.rgb)/255.0,vec3(0.0)),vec3(2.2)),float(a_rgba.a)/255.0);"
"vec3 cwp=apply_world_curve(a_pos,su.sky_zenith_pad,su.tile_overlay[41]);"
"gl_Position=su.view_proj*vec4(cwp,1.0);"
"}";

static const char *k_gles_fs_vfx =
"in vec2 v_uv;\nin vec4 v_col;\nin vec2 v_kp;"
"uniform sampler2D tex_depth;"
"out vec4 o_col;"
"void main(){"
"ivec2 dim=textureSize(tex_depth,0);"
"float scene_z=texelFetch(tex_depth,clamp(ivec2(gl_FragCoord.xy),ivec2(0),dim-1),0).r;"
"float lin_s=linear_eye_z(scene_z),lin_v=linear_eye_z(gl_FragCoord.z);"
"int k=int(v_kp.x+0.5);"
"if(k==4){"
"if(lin_v > lin_s+0.045) discard;"
"vec2 p=v_uv*2.0-1.0;"
"float sa=v_col.a*pow(SAT(1.0-dot(p,p)),1.50)*rsmooth(0.30,0.04,abs(lin_s-lin_v));"
"o_col=vec4(vec3(0.008,0.014,0.034)*sa,sa);return;"
"}"
"if(gl_FragCoord.z > scene_z) discard;"
"float a=v_col.a*smoothstep(0.0,0.18,lin_s-lin_v);"
"vec3 c=v_col.rgb*3.0;"
"if(k==0){ float bl=sin(v_uv.x*3.14159)*pow(v_uv.y,1.6);c=mix(c,vec3(4.2,4.1,3.9),smoothstep(0.68,0.98,v_uv.y)*sin(v_uv.x*3.14159));a *= bl;}"
"else if(k==1){ vec2 p=v_uv*2.0-1.0;float r=length(p),ang=atan(p.y,p.x);"
"a *= SAT(rsmooth(0.05,0.01,abs(r-0.88))+rsmooth(0.04,0.01,abs(r-0.64))+rsmooth(0.05,0.01,abs(r-(0.58+0.22*cos(ang*6.0))))+step(0.3,sin(ang*24.0))*rsmooth(0.09,0.02,abs(r-0.76))*0.85);c *= 1.25;}"
"else if(k==3){ float wv=v_uv.y+0.16*sin(v_uv.x*6.28318);float core=pow(sin(v_uv.x*3.14159),2.0)*smoothstep(0.0,0.45,wv)*rsmooth(1.0,0.55,wv);c=mix(v_col.rgb*1.15,vec3(1.45,1.48,1.52),core*0.5);a *= core;}"
"else { a *= rsmooth(1.0,0.15,length(v_uv*2.0-1.0));c *= 0.82;}"
"o_col=vec4(c*a,a);"
"}";

static const char *k_gles_fs_bloom_down =
"in vec2 v_uv;\nout vec4 o_col;"
"uniform sampler2D u_src;\nuniform vec4 u_par;"
"void main(){"
"vec2 off=u_par.xy;float th=u_par.z;"
"vec3 c=bloom_karis_tap(texture(u_src,v_uv).rgb,th)*0.5\n"
"+ bloom_karis_tap(texture(u_src,v_uv+vec2(-off.x,-off.y)).rgb,th)*0.125\n"
"+ bloom_karis_tap(texture(u_src,v_uv+vec2( off.x,-off.y)).rgb,th)*0.125\n"
"+ bloom_karis_tap(texture(u_src,v_uv+vec2(-off.x,off.y)).rgb,th)*0.125\n"
"+ bloom_karis_tap(texture(u_src,v_uv+vec2( off.x,off.y)).rgb,th)*0.125;"
"o_col=vec4(c,1.0);"
"}";

static const char *k_gles_fs_composite =
"in vec2 v_uv;\nout vec4 o_col;"
"uniform sampler2D hdr_tex;"
"uniform sampler2D bloom1;"
"uniform sampler2D bloom2;"
"uniform sampler2D bloom3;"
"uniform sampler2D tex_depth;"
"void main(){"
"ivec2 dim=textureSize(tex_depth,0);"
"ivec2 p0=clamp(ivec2(gl_FragCoord.xy),ivec2(0),dim-1);"
"float z0=texelFetch(tex_depth,p0,0).r;"
"float d0=linear_eye_z(z0);"
"float coc=0.0;"
"if(z0 < 0.9998){"
"float fd=max(su.tile_overlay[41].z,2.5);"
"coc=mix(smoothstep(fd+13.5,fd+28.0,d0)*0.52,smoothstep(fd+2.4,fd+10.5,d0)*0.85,su.tile_overlay[41].w);"
"}"
"vec3 hdr=texture(hdr_tex,v_uv).rgb;"
"if(coc > 0.02){"
"vec3 acc=hdr;float wsum=1.0,ang0=ign21(gl_FragCoord.xy)*6.283185;"
"vec2 b_rad=su.screen_params.zw*(coc*4.8);"
"for(int i=0;i<6;i++){"
"float a=ang0+float(i)*2.399963,r=sqrt((float(i)+0.5)/6.0);"
"vec2 off=vec2(cos(a),sin(a))*r*b_rad;"
"float td=linear_eye_z(texelFetch(tex_depth,clamp(p0+ivec2(off*su.screen_params.xy),ivec2(0),dim-1),0).r);"
"float w=smoothstep(-1.2,0.2,td-d0);"
"acc += texture(hdr_tex,clamp(v_uv+off,0.001,0.999)).rgb*w;wsum += w;"
"}"
"hdr=acc/wsum;"
"}"
"if(z0 < 0.9995&&d0 < 32.0){"
"float ang=ign21(gl_FragCoord.xy+vec2(17.0,31.0))*3.14159,occ=0.0;"
"for(int i=0;i<4;i++){"
"float a=ang+float(i)*0.785398;"
"float px_r=((i & 1)==0?5.5:13.5)*clamp(10.0/max(d0,2.5),0.55,2.2);"
"ivec2 st=ivec2(round(vec2(cos(a),sin(a))*px_r));"
"float dp=linear_eye_z(texelFetch(tex_depth,clamp(p0+st,ivec2(0),dim-1),0).r);"
"float dn=linear_eye_z(texelFetch(tex_depth,clamp(p0-st,ivec2(0),dim-1),0).r);"
"float valley=d0-0.5*(dp+dn);"
"occ += SAT((valley-0.014)*3.8)*rsmooth(0.95,0.16,max(abs(d0-dp),abs(d0-dn)));"
"}"
"float ao=1.0-SAT(occ*0.28)*rsmooth(32.0,18.0,d0)*0.34;"
"hdr *= mix(vec3(0.86,0.90,0.95)*ao,vec3(1.0),ao);"
"}"
"vec3 bl=texture(bloom1,v_uv).rgb*0.42+texture(bloom2,v_uv).rgb*0.58+texture(bloom3,v_uv).rgb*0.72;"
"vec2 sun_uv=su.sun_screen_pos.xy;"
"if(su.sun_screen_pos.z > 0.01){"
"vec2 delta=(sun_uv-v_uv)*(0.72/12.0),ruv=v_uv;"
"vec3 rays=vec3(0.0);float dec=1.0;"
"for(int i=0;i<12;i++){ ruv += delta;rays += texture(bloom1,clamp(ruv,0.002,0.998)).rgb*dec;dec *= 0.88;}"
"hdr += rays*vec3(1.0,0.82,0.52)*(0.042*su.sun_screen_pos.z);"
"}"
"vec3 ldr=agx_tonemap(hdr+bl*su.sun_screen_pos.w);"
"vec2 vd=v_uv-0.5;"
"o_col=vec4(ldr*(1.0-0.18*dot(vd,vd)),1.0);"
"}";

static const char *k_gles_vs_ui =
"layout(location=0) in vec4 i_rect;\nlayout(location=1) in vec4 i_uvr;"
"layout(location=2) in uvec2 i_cols;\nlayout(location=3) in vec2 i_rb;\nlayout(location=4) in uvec2 i_kb;"
"out vec2 v_px;\nout vec2 v_uv;\nflat out vec4 v_rect;\nflat out vec4 v_c0;\nflat out vec4 v_c1;\nflat out vec2 v_rb;\nflat out uvec2 v_kb;"
"vec4 unpack_rgba(uint c){ return vec4(float(c&255u),float((c>>8)&255u),float((c>>16)&255u),float((c>>24)&255u))/255.0;}"
"void main(){"
"int vid=gl_VertexID;"
"vec2 c=vec2((vid==1||vid==2||vid==4)?1.0:0.0,(vid>=2&&vid!=3)?1.0:0.0);"
"vec2 pad=(i_kb.x==1u)?vec2(2.0):vec2(0.0);"
"vec2 px=mix(i_rect.xy-pad,i_rect.zw+pad,c);"
"v_px=px;v_uv=mix(i_uvr.xy,i_uvr.zw,c);v_rect=i_rect;v_c0=unpack_rgba(i_cols.x);v_c1=unpack_rgba(i_cols.y);v_rb=i_rb;v_kb=i_kb;"
"vec2 ndc=vec2((px.x*su.screen_params.z)*2.0-1.0,1.0-(px.y*su.screen_params.w)*2.0);"
"gl_Position=vec4(ndc,0.0,1.0);"
"}";

static const char *k_gles_fs_ui =
"in vec2 v_px;\nin vec2 v_uv;\nflat in vec4 v_rect;\nflat in vec4 v_c0;\nflat in vec4 v_c1;\nflat in vec2 v_rb;\nflat in uvec2 v_kb;"
"out vec4 o_col;"
"float glyph_sdf(vec2 uv,uint bits){"
"vec2 gp=uv*vec2(5.0,6.0)-0.5;"
"float md=4.0;"
"for(int r=0;r<6;r++) for(int c=0;c<5;c++){"
"uint idx=uint(r*5+c);"
"if(((bits>>idx)&1u)!=0u){"
"vec2 center=vec2(float(c),float(r));"
"md=min(md,length(gp-center));"
"if(c<4&&((bits>>(idx+1u))&1u)!=0u){ vec2 pa=gp-center,ba=vec2(1.0,0.0);md=min(md,length(pa-ba*SAT(dot(pa,ba))));}"
"if(r<5&&((bits>>(idx+5u))&1u)!=0u){ vec2 pa=gp-center,ba=vec2(0.0,1.0);md=min(md,length(pa-ba*SAT(dot(pa,ba))));}"
"if(c<4&&r<5&&((bits>>(idx+6u))&1u)!=0u){ vec2 pa=gp-center,ba=vec2(1.0,1.0);md=min(md,length(pa-ba*SAT(dot(pa,ba)*0.5)));}"
"if(c>0&&r<5&&((bits>>(idx+4u))&1u)!=0u){ vec2 pa=gp-center,ba=vec2(-1.0,1.0);md=min(md,length(pa-ba*SAT(dot(pa,ba)*0.5)));}"
"}"
"}"
"return md;"
"}"
"void main(){"
"if(v_kb.x==1u){"
"vec2 uv=(v_px-v_rect.xy)/max(v_rect.zw-v_rect.xy,vec2(1.0));"
"float a_fg=rsmooth(0.56,0.34,glyph_sdf(uv,v_kb.y));"
"float a_sh=rsmooth(0.68,0.36,glyph_sdf(uv-vec2(0.06,0.07),v_kb.y))*v_c1.a;"
"o_col=vec4(mix(v_c1.rgb,v_c0.rgb,a_fg),SAT(max(a_sh,a_fg*v_c0.a)));"
"return;"
"}"
"vec2 center=0.5*(v_rect.xy+v_rect.zw),half_sz=0.5*(v_rect.zw-v_rect.xy);"
"float rad=min(v_rb.x,min(half_sz.x,half_sz.y));"
"vec2 q=abs(v_px-center)-(half_sz-rad);"
"float sdf=length(max(q,0.0))+min(max(q.x,q.y),0.0)-rad;"
"float alpha=rsmooth(0.8,-0.8,sdf);"
"if(alpha<=0.002) discard;"
"vec4 col=v_c0;"
"col.rgb *= (1.14-0.24*SAT((v_px.y-v_rect.y)/max(1.0,v_rect.w-v_rect.y)));"
"if(v_rb.y > 0.1) col=mix(col,v_c1,smoothstep(-v_rb.y-0.8,-v_rb.y+0.6,sdf));"
"col.a *= alpha;"
"o_col=col;"
"}";
#endif // __ANDROID__

#endif // ENG96K_SHADERS_H
