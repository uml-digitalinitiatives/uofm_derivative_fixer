<?php

function uofm_derivative_fixer_drush_command() {
  $items = array();

  $items['uofm_derivative_fix_preprocess'] = array(
    'options' => array(
       'pid' => array(
          'description' => 'A PID to re-index',
        ),
        'pidlist' => array(
          'description' => 'A comma seperated list of PIDs to re-index',
        ),
        'pidfile' => array(
          'description' => 'Path to a textfile of PIDs to re-index, one per line',
        ),
    ),
    'aliases' => array('uofm_derivative_pp'),
    'callback' => 'uofm_derivative_fixer_preprocess',
    'description' => '',
    'drupal dependencies' => array('islandora', 'islandora_solr', 'islandora_collection_search'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  
  return $items;
}

function uofm_derivative_fixer_preprocess() {
  if (is_null(drush_get_option('pid', NULL)) && is_null(drush_get_option("pidlist", NULL)) && is_null(drush_get_option("pidfile", NULL))) {
    drush_set_error('no value', 'You must enter one of --pid, --pidlist or --pidfile');
  }
  $results = array();
  if (!is_null(drush_get_option('pid', NULL))) {
     $results[] = drush_get_option('pid', "");
  }
  else if (!is_null(drush_get_option('pidlist', NULL))) {
    $list = drush_get_option('pidlist', "");
    $tmpRes = explode($list, ',');
    foreach ($tmpRes as $t) {
      if (strlen(trim($t)) > 0) {
        $results[] = trim($t);
      }
    }
  }
  else if (!is_null(drush_get_option('pidfile', NULL))) {
    if (file_exists(drush_get_option('pidfile', ''))) {
      $fp = @fopen(drush_get_option('pidfile', ''), 'r');
      if (!$fp) {
        drush_set_error("Unable to open file " . drush_get_option('pidfile', ''));
      }
      while (!feof($fp)) {
        $line = trim(fgets($fp, 4096));
        if (is_array($line)) {
          ob_start();
          var_dump($line);
          $x = ob_get_clean();
          drush_set_error("Trying to process an array ($x)");
        }
        if (!empty($line)) {
          $results[] = trim($line);
        }
      }
    }
    else {
      drush_set_error("File: " . drush_get_option('pidfile', '') . " does not exist.");
    }
  }
  $queue = DrupalQueue::get(UOFM_DER_FIX_QUEUE);
  $count = 0;
  foreach ($results as $obj) {
    if (strlen(trim($obj)) > 0) {
      $queue->createItem($obj);
      $count += 1;
    }
  }
  drush_log('Added '. $count . ' items to queue', 'ok');
}
