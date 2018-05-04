#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <rpc/des_crypt.h>




static const unsigned char map[256] =
{
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 253, 255,255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
	52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255, 254, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,
	7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
	255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
	49,  50,  51, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  255, 255, 255, 255
};

typedef struct{
  ngx_str_t des_key;
  ngx_str_t end_char;                  //支持多字符串
  ngx_str_t ignore_endchar;
  ngx_str_t str_rule;
  ngx_str_t str_replace;
}ngx_http_redef_url_loc_conf_t;

static char* ngx_http_redef_url( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static void* ngx_http_redef_url_create_loc_conf( ngx_conf_t* cf );
static ngx_int_t ngx_http_redef_url_init(ngx_conf_t *cf);
static char* ngx_http_redef_url_end( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static char* ngx_http_redef_url_end_switch( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static char* ngx_http_redef_url_rule( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static char* ngx_http_redef_url_replace( ngx_conf_t* cf, ngx_command_t* cmd, void* conf );
static int des_decrypt(char *key, char *data, int len);

static int base64Decode(char * in,int inlen);
static int replaceChar( char* in);



static ngx_command_t ngx_http_redef_url_commands[] = {
  {
    ngx_string( "des_key" ),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_http_redef_url,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_redef_url_loc_conf_t, des_key),
    NULL
  },
  {
    ngx_string( "end_char" ),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_http_redef_url_end,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_redef_url_loc_conf_t, end_char),
    NULL
  },
    {
    ngx_string( "ignore_endchar" ),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_http_redef_url_end_switch,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_redef_url_loc_conf_t, ignore_endchar),
    NULL
  },
  {
    ngx_string( "rule" ),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_http_redef_url_rule,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_redef_url_loc_conf_t,str_rule),
    NULL
  },
  {
    ngx_string( "replace" ),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_http_redef_url_replace,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_redef_url_loc_conf_t, str_replace),
    NULL
  },
  ngx_null_command
};

static ngx_http_module_t ngx_http_redef_url_module_ctx =  {

  NULL,
  ngx_http_redef_url_init,

  NULL,
  NULL,

  NULL,
  NULL,

  ngx_http_redef_url_create_loc_conf,
  NULL
};

ngx_module_t ngx_http_redef_url_module = {
  NGX_MODULE_V1,
  &ngx_http_redef_url_module_ctx,
  ngx_http_redef_url_commands,
  NGX_HTTP_MODULE,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_redef_url_handler( ngx_http_request_t* r )
{                                           																		
	ngx_http_redef_url_loc_conf_t* hlcf;
	hlcf = ngx_http_get_module_loc_conf( r, ngx_http_redef_url_module );
 
	if( NULL == hlcf->des_key.data)															//是否有密钥
	{	
		return NGX_DECLINED;
	}
	
	ngx_int_t nlen = r->unparsed_uri.len;													//nginx中内存是在池里面，它的内存是根据大小相等等特性，是可以复用的。
	u_char* url = ngx_palloc( r->pool,nlen+1 );												//所以申请内存的时候，要根据实际情况添加结束符的占位。否则url会在cpy后
	ngx_memzero(url, nlen+1);
	ngx_memcpy(url,  r->unparsed_uri.data,nlen);	
	
	char* endchar = ngx_palloc( r->pool,hlcf->end_char.len+1 );	
	ngx_memzero(endchar, hlcf->end_char.len+1);	
	ngx_memcpy(endchar, hlcf->end_char.data,hlcf->end_char.len);	
	u_char* param = (u_char*)strstr((char*)url,endchar);
	if(param)
	{
		*param = '\0';
	}
	
	u_char * code = (u_char *)strrchr( (char*)url, '/' );
	code++;
	

	//取出来base的编码字符串
	ngx_int_t codelen = ngx_strlen(code);
	u_char* dst = ngx_palloc( r->pool,codelen+1);		//解密的内容存在这里
	ngx_memzero( dst,codelen+1);	
	
	char* src = (char*)code;
	replaceChar( (char*)code );
	ngx_memcpy( dst, src, codelen+1 );
	
	ngx_int_t debas_len =  base64Decode( (char*)dst, ngx_strlen(dst));  //base转码
	if( -1 == debas_len )
	{
		ngx_log_error( NGX_LOG_EMERG  , r->connection->log, 0,"debas error!\n" );
		return NGX_ERROR;	
	}
	
	if( -1 == des_decrypt( (char*)hlcf->des_key.data, (char*)dst, debas_len ) )    		//des解密
	{
		ngx_log_error( NGX_LOG_EMERG  , r->connection->log, 0,"descrypt error please check the key is right or not!\n" );
		return NGX_ERROR;																		
	}

	ngx_int_t nlast = (ngx_int_t)dst[ debas_len - 1 ];	//去掉补位 ,补位为8进制ascii 1-7直接强转
	if(  nlast > 0 && nlast < 9 ) //需要去掉补位
	{
		dst[debas_len-nlast] = '\0';
		debas_len = debas_len - nlast;
	}	
	
	ngx_int_t uri_len = 0;                               //长度
	ngx_memcpy(url+1,dst,debas_len);
	*url = '/';
	debas_len += 1;


       if(hlcf->str_rule.len != 0)
       {
	//hlcf->str_replace;
	u_char* prule =  ngx_palloc( r->pool,hlcf->str_rule.len+1); 
	ngx_memzero(prule,hlcf->str_rule.len+1);
	ngx_memcpy(prule,hlcf->str_rule.data,hlcf->str_rule.len+1);
	prule[hlcf->str_rule.len+1]='\0';
	u_char* pbuf = 	(u_char*)ngx_strstr((char*)prule,";");
	while(pbuf != NULL )
	{
	  pbuf[0]='\0';
	  if(ngx_strstr(url,prule)!=NULL)
	    {
	      u_char* preplace = hlcf->str_replace.data;
	      uri_len = hlcf->str_replace.len;
	      preplace[uri_len+1]= '\0';
	      ngx_memcpy(url,preplace,uri_len);
	      uri_len = hlcf->str_replace.len ;
	      url[uri_len] = '\0';
	      debas_len = uri_len;
	      break;
	    }
	  prule = pbuf +strlen(";");
	  pbuf = (u_char*)ngx_strstr((char*)prule,";");
	}
       }
		
	if(param)
	{
		*param = *endchar;		
		u_char * origparam = param;
		if( strchr( (char*)dst,'?') )						//替换处理
		{		
			while( *param !=0 )
            {
				if( *param == '?')
					*param = '&';
				param++;
			}
		}	
		
		if( strncmp((char*)hlcf->ignore_endchar.data,"off", 3) ) // ignore_endchar
		{
			origparam += hlcf->end_char.len;			
		}
		ngx_memcpy(url+debas_len, origparam, ngx_strlen(origparam));		
		debas_len += ngx_strlen(origparam);	
	}
	url[debas_len] = '\0';

	
	u_char* f_exten_end =  (u_char*)ngx_strchr( (char*)url,'?');	
	u_char* f_exten_start=NULL;

	if( f_exten_end )
	{	
		ngx_int_t end = f_exten_end -url;
		u_char* f_exten = ngx_palloc( r->pool,end+1);		//解密的内容存在这里
		ngx_memzero( f_exten,end+1);	
		ngx_memcpy(f_exten,url,end);
		f_exten_start = (u_char*)strrchr( (char*)f_exten,'.'); //求后缀名否则视频会以下载形式打开而不是播放    
		if(f_exten_start)
		{
			ngx_str_set(  &r->exten, f_exten_start+1 );		
			r->exten.len =  ngx_strlen(f_exten_start) - 1;
		}
		uri_len = end;
	}	
	else
	{
		f_exten_start = (u_char*)strrchr( (char*)url,'.'); //求后缀名否则视频会以下载形式打开而不是播放    
		if(f_exten_start)
		{
			ngx_str_set(  &r->exten, f_exten_start+1 );		
			r->exten.len = ngx_strlen(f_exten_start) -1;
		}
		uri_len = ngx_strlen(url);
	}
	

	

	ngx_str_null( & r->uri );		
	ngx_str_set(  &r->uri, url ); //转码后的url放回请求中即可
	r->uri.len = uri_len;		

	return NGX_DECLINED;
}

static ngx_int_t
ngx_http_redef_url_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_redef_url_handler;

    return NGX_OK;
}

static void*
ngx_http_redef_url_create_loc_conf( ngx_conf_t* cf )
{
  ngx_http_redef_url_loc_conf_t* conf;
  conf = ngx_palloc( cf->pool, sizeof( ngx_http_redef_url_loc_conf_t) );
  if( NULL == conf )
    {
      return NGX_CONF_ERROR;
    }
   ngx_str_null(&conf->des_key);
   ngx_str_null(&conf->end_char);
   ngx_str_null(&conf->ignore_endchar);
   ngx_str_null(&conf->str_rule);
   ngx_str_null(&conf->str_replace);
   return conf;
}

static char *
ngx_http_redef_url(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}
static char*ngx_http_redef_url_end( ngx_conf_t* cf, ngx_command_t* cmd, void* conf )
{
	ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}
static char* ngx_http_redef_url_end_switch( ngx_conf_t* cf, ngx_command_t* cmd, void* conf )
{
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}
static char* ngx_http_redef_url_rule( ngx_conf_t* cf, ngx_command_t* cmd, void* conf )
{
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}
static char* ngx_http_redef_url_replace( ngx_conf_t* cf, ngx_command_t* cmd, void* conf )
{
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}

static int replaceChar( char* in)
{
	while( *in !=0 )
	{
		if( *in == '.' )
		    *in = '+';
		if( *in == '-' )
		    *in ='/';
		if( *in == '_' )
		   *in = '=';
		in++;
	}
	return 0;
}

static int base64Decode(char * in,int inlen)
{
	unsigned char * out = (unsigned char *)malloc(inlen);
	unsigned long t, x, y, z;
	unsigned char c;
	int g = 3;

	for (x = y = z = t = 0; x<(unsigned long)inlen;)
		{
			c = map[(unsigned char)in[x++]];
			if (c == 255) return -1;
			if (c == 253) continue;
			if (c == 254)
				{
					c = 0;
					g--;
				}
			t = (t<<6)|c;
			if (++y == 4)
				{
					out[z++] = (unsigned char)((t>>16)&255);
					if (g > 1) out[z++] = (unsigned char)((t>>8)&255);
					if (g > 2) out[z++] = (unsigned char)(t&255);
					y = t = 0;
				}
		}
	memcpy(in,out,z);
	free(out);
	return z;
}


static int
des_decrypt( char *key, char *data, int len)
{
	char pkey[8];
	strncpy(pkey, key, 8);
	des_setparity(pkey);
	ngx_int_t n_res =  ecb_crypt(pkey, data, len, DES_DECRYPT) ;
	if( n_res == DESERR_NONE || n_res == DESERR_NOHWDEVICE )
	{
		return 1;
	}
	else
	{
		return -1;
	}
}
