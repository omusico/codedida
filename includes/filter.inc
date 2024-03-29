<?php
// $Id$

/**
 * 格式化内容
 * @param (string) $text
 *  原始字符串
 * @param (int) $format
 *  输入格式 id
 * @param (string) $type
 *  自定义类型，一般用于缓存
 * @param (int) $id
 *  自定义 id，一般用于缓存
 */
function filter_view($text, $format = NULL, $type = NULL, $id = NULL) {
  global $conf;

  if (!isset($format) || !$conf['formats'][$format]) {
    $format = $conf['default_format'];
  }
  
  $options = filter_user_format();
  
  // 是否缓存内容，当内容不发生变更，下次请求将直接从数据库读取缓存。这将减少 PHP 开销，却会增加数据库查询
  if ($conf['filter_cache']) {
  
    // 含有 php 过滤器，则不缓存
    
    if ($cache = !filter_is_php($format)) {
      $cache_id = $format .':'. md5($text);
      if ($cached = cache_get($cache_id, 'cache_filter')) {
        
        // 代码高亮，加载 js
        if (filter_is_filter($format, 'system', 'highlighter')) {
          dd_get_highlighter();
        }
        
        return $cached->data;
      }
    }
    
  } else {
    $cache = false;
  }
  
  if ($conf['filters'][$format] && is_array($conf['filters'][$format])) {
    foreach ($conf['filters'][$format] as $filter) {
      $text = call_user_func($filter['module'] . '_filter', 'view', $filter['delta'], $filter['format'], $text, $filter['filter'], $type, $id);
    }
  }
  
  if ($cache) {
    cache_set($cache_id, $text, 'cache_filter');
  }
  
  return $text;
}

/**
 * PHP 解析
 */
function filter_tags_php($text) {
  if ($GLOBALS['_filter_is_php_code'] == true) {
    // 全局启用 php code
    if (strpos($text, '<?php') !== false) {
      return preg_replace_callback('/<\?php(.*?)\?>$/ms', '_filter_tags_php', $text);
    }
  } else {
    return $text;
  }
}

function _filter_tags_php($matches) {
  if ($matches[1]) {
    ob_start();
    eval($matches[1]);
    $text = ob_get_contents();
    ob_end_clean();
  }
  return $text;
}

/**
 * 获取当前用户输入格式
 */
function filter_user_format($ac = NULL) {
  global $conf;
  $options = array();
  
  if ($conf['formats']) {
    if (!$ac) {
      $ac = $GLOBALS['user'];
    }
    if ($ac->uid != 1) {
      $fids = array();
      if ($conf['default_format']) {
        $fids[] = $conf['default_format'];
      }
      if ($ac->roles) {
        foreach ($ac->roles as $rid => $c) {
          if (is_array($conf['format_roles'][$rid])) {
            $fids = array_merge($fids, $conf['format_roles'][$rid]);
          }
        }
      }
      if ($fids) {
        foreach ($conf['formats'] as $f) {
          if (in_array($f['format'], $fids)) {
            $options[$f['format']] = $f['name'] .'<br /><span class="description form_description">'. check_plain(t('system', $f['description'])) .'</span><br />';
          }
        }
      }
    } else {
      foreach ($conf['formats'] as $f) {
        $options[$f['format']] = $f['name'] .'<br /><span class="description form_description">'. check_plain(t('system', $f['description'])) .'</span><br />';
      }
    }
  }
  return $options;
}

/**
 * 输入格式
 */
 
function filter_form_field($format = NULL, $required = false, $title = NULL) {
  global $user, $conf;
  if (!isset($format) || !$conf['formats'][$format]) {
    $format = $conf['default_format'];
  }
  $option = filter_user_format();
  if (count($option) > 1) {
    $field = array(
      '#fieldset_prefix' => 'asc',
      '#fieldset_legend' => $title ? $title : t('system', '输入格式'),
      '#type' => 'radio',
      '#required' => $required,
      '#default_value' => $format,
      '#fieldset_suffix' => 1,
      '#options' => filter_user_format()
    );
  } else {
    $field = array(
      '#type' => 'hidden',
      '#default_value' => $format,
      '#constant' => 1
    );
  }
  return $field;
}

/**
 * 输入格式、角色格式配置缓存
 */
function filter_set_cache() {
  if ($fetch = db_query('SELECT roles, format, name, description FROM {filter_format}', NULL, array('fetch' => 'array'))) {
    $roles = array();
    foreach ($fetch as $o) {
      if ($o['roles']) {
        $o['roles'] = unserialize($o['roles']);
        if (is_array($o['roles'])) {
          foreach ($o['roles'] as $rid) {
            $roles[$rid][$o['format']] = $o['format'];
          }
        }
      }
      $formats[$o['format']] = $o;
    }
  }
  
  if ($fetch = db_query('SELECT filter, format, delta, module FROM {filters} ORDER BY weight ASC, fid DESC', NULL, array('fetch' => 'array'))) {
    foreach ($fetch as $o) {
      $filters[$o['format']][] = $o;
    }
  }
  
  cache_del('cid', 'filter_get_filters');
  var_set('formats', $formats, 0);
  var_set('filters', $filters, 0);
  var_set('format_roles', $roles, 0);
  cache_del('empty', NULL, 'cache_filter');
  
  var_init();
}

/**
 * 检查输入法中是否含有 php 过滤器
 */
function filter_is_php($format) {
  return filter_is_filter($format, 'system', 'php');
}

/**
 * 
 * 检查输入法中是否含有某过滤器
 * @param (int) $format
 *  输入法
 * @param (string) $module
 *  模块名称
 * @param (*) $delta
 *  过滤器 delta
 */
function filter_is_filter($format, $module, $delta) {
  global $conf;
  if ($conf['filters'] && $conf['filters'][$format]) {
    foreach ($conf['filters'][$format] as $f) {
      if ($f['module'] == $module && $f['delta'] == $delta) {
        return true;
      }
    }
  }
  
  return false;
}

/**
 * 获取自定义过滤器
 */
function filter_get_filters($fid = NULL) {
  static $filters;
  
  if (!isset($filters)) {
    if ($cache = cache_get('filter_get_filters')) {
      $filters = $cache->data;
    } else {
      if ($fetch = db_query('SELECT * FROM {filter_tags}')) {
        foreach ($fetch as $o) {
          if ($o->module == 'system' && $o->body) {
            switch ($o->delta) {
              case 0: case 1:
                if ($tags = explode(' ', $o->body)) {
                  foreach ($tags as $tag) {
                    $o->tags[$tag] = 1;
                    $o->strip_tags .= '<'.$tag.'>';
                  }
                }
              break;
              case 2:
                if ($tags = explode("\n", $o->body)) {
                  foreach ($tags as $tag) {
                    if (strpos($tag, '|') === false) {
                      $o->tags[$tag] = "";
                    } else {
                      $t = explode('|', $tag, 2);
                      $o->tags[$t[0]] = $t[1];
                    }
                  }
                }
              }
          }
          $filters[$o->fid] = $o;
        }
      }
      cache_set('filter_get_filters', $filters);
    }
  }
  return $fid ? $filters[$fid] : $filters;
}
