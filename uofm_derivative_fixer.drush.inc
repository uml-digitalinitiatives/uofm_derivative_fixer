<?php
/**
 * @file
 * Drush commands to add items to
 * have derivatives generated according to
 * content models
 */

/**
 * Implements hook_drush_command().
 */
function uofm_derivative_fixer_drush_command() {
  $items = array();

  $items['uofm_derivative_fix_preprocess'] = array(
    'options' => array(
      'query' => array(
        'description' => 'The SPARQL where clause, uses "?object" as the returned variable.',
      ),
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
    'aliases' => array('uofm_dfp'),
    'callback' => 'uofm_derivative_fixer_preprocess',
    'description' => 'Add a list of objects to a queue to have any missing derivatives generated for.',
    'drupal dependencies' => array(
      'islandora',
      'islandora_solr',
      'islandora_collection_search',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  $items['uofm_derivative_fix_run'] = array(
    'options' => array(
      'timeout' => array(
        'description' => t('How long to run the queue for in seconds, otherwise run until queue is empty.'),
      ),
    ),
    'aliases' => array('uofm_dfr'),
    'callback' => 'uofm_derivative_fixer_run',
    'description' => 'Run the derivative queue in a batch instead of waiting for cron runs to complete it.',
    'drupal dependencies' => array('islandora'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );

  return $items;
}

/**
 * Drush command to add pids to the queue if valid.
 */
function uofm_derivative_fixer_preprocess() {
  module_load_include('inc', 'islandora', 'includes/utilities');

  if (is_null(drush_get_option('query', NULL)) && is_null(drush_get_option('pid', NULL)) && is_null(drush_get_option("pidlist", NULL)) && is_null(drush_get_option("pidfile", NULL))) {
    drush_set_error('no value', 'You must enter one of --query, --pid, --pidlist or --pidfile');
  }
  $results = array();
  if (!is_null(drush_get_option('query', NULL))) {
    $query_param = drush_get_option('query');
    $results = __uofm_derivative_fixer_sparql_query($query_param);
  }
  elseif (!is_null(drush_get_option('pid', NULL))) {
    $results[] = drush_get_option('pid', "");
  }
  elseif (!is_null(drush_get_option('pidlist', NULL))) {
    $list = drush_get_option('pidlist', "");
    $tmp_res = explode(',', $list);
    foreach ($tmp_res as $t) {
      if (strlen(trim($t)) > 0) {
        $results[] = trim($t);
      }
    }
  }
  elseif (!is_null(drush_get_option('pidfile', NULL))) {
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
  $queue = DrupalQueue::get(UOFM_DERIVATIVE_FIXER_QUEUE);
  $count = 0;
  foreach ($results as $obj) {
    if (strlen(trim($obj)) > 0 && islandora_is_valid_pid($obj)) {
      $queue->createItem($obj);
      $count += 1;
    }
  }
  drush_log("Added $count items to queue", 'ok');
}

/**
 * Perform our Sparql query and return an array of PIDs.
 */
function __uofm_derivative_fixer_sparql_query($where) {
  drupal_static_reset('islandora_get_tuque_connection');
  $connection = islandora_get_tuque_connection();

  $query = 'select ?object from <#ri> where { ';
  $query .= $where;
  $query .= ' }';

  $temp = $connection->repository->ri->sparqlQuery($query, 'unlimited');

  $get_pid = function($o) {
    if (is_array($o) && array_key_exists('object', $o)) {
      return $o['object']['value'];
    }
    else {
      ob_start();
      print_r($o);
      $x = ob_get_clean();
      drush_log("Error trying to get value of sparql object ($x)", 'error');
    }
  };

  return array_map($get_pid, $temp);
}

/**
 * Execute the callback function for the queue in a batch.
 */
function uofm_derivative_fixer_run() {
  $timeout = drush_get_option('timeout', ini_get('max_execution_time'));
  $queue = DrupalQueue::get(UOFM_DERIVATIVE_FIXER_QUEUE);
  timer_start(UOFM_DERIVATIVE_FIXER_TIMER);
  while (($timeout == 0 || (timer_read(UOFM_DERIVATIVE_FIXER_TIMER) / 1000) < $timeout) && $queue->numberOfItems() > 0) {
    $item = $queue->claimItem(300);
    uofm_derivative_fixer_process($item);
    $queue->deleteItem($item);
    drush_log("Processed object ({$item->data})", "ok");
  }
  timer_stop(UOFM_DERIVATIVE_FIXER_TIMER);
}
