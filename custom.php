<?php
// $Id$

if(!$_GET['module']) die('byebye');
require_once './includes/bootstrap.inc';

header('charset=utf-8');

$includes = NULL;

if($_GET['includes']){
  if(strpos($_GET['includes'], '|') !== false){
    $includes = explode('|', $_GET['includes']);
  }else{
    $includes[] = $_GET['includes'];
  }
}

bootstrap('custom', $includes);

if($GLOBALS['conf']['modules'][$_GET['module']]){
  // 加载模块主文件
  require_once dd_get_path('module', $_GET['module']).'/'.$_GET['module'].'.module';
  
  // 检查函数是否存在
  $function = $_GET['module'].'_call_custom';
  
  if(function_exists($function)){
    $function($_GET);
  }else{
    echo 'byebye';
  }
}