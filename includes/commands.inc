<?php
abstract class command{
    protected   $pid;
    protected   $dry;
    protected   $object;
    protected   $message;
    protected   $stop;
    
    public function __construct($pid, $dry, $options) {
        $this->pid = $pid;
        $this->dry = $dry;
        $this->options = $options;
        $this->message = '';
        $this->stop = false;
    }

    public function exec(){
        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        
        if ($this->stop == false)
            $this->_dry_exec();
        
        if (($this->dry == false) && ($this->stop == false))
            $this->_exec();
        
        if ($this->dry == true)
            $this->message = "DRY RUN - ".$this->message;
        
        return $this->message;
    }
    
    abstract protected function _dry_exec();
    
    abstract protected function _exec();
        
    protected function retrieve_islandora_object(){
        $this->object = islandora_object_load($this->pid);
    }  
    
    protected function get_pid(){
        return $this->pid;
    }
    
    protected function has_children (){
        $query = <<<EOQ
ASK {
  ?subject <fedora-model:hasModel> <info:fedora/fedora-system:FedoraObject-3.0> ;
           ?pred <info:fedora/$this->pid> .
}
EOQ;
        
        $child_results = $this->object->repository->ri->sparqlQuery($query);
        return $child_results[0]['k0']['value'] == 'true';
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/

class edit_state extends command{
    protected function _dry_exec(){
        //        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        elseif ($this->has_children()){
            $this->message = $this->get_pid().': Has children. Cannot edit';
            $this->stop = true;
        }
        else{
            $this->message = $this->get_pid().': Editing...';
        }
    }
    
    protected function _exec(){
        $this->object->state=$this->options['state'];
        $this->message.=' State edited';
    }    
    
}
/********************************************************************
 *
 *
 *
 ********************************************************************/

class purge extends command{

    protected function _dry_exec(){
//        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        elseif ($this->has_children()){
            $this->message = $this->get_pid().': Has children. Cannot purge';
            $this->stop = true;
        }
        else{
            $this->message = $this->get_pid().': Purging...';
        }
    }

    protected function _exec(){
        $this->object->repository->purgeObject($this->pid);
        $this->message.=' Purged';
    }    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/

class label_decode extends command{
    
    protected function _dry_exec(){
        //        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        elseif ($this->has_children()){
            $this->message = $this->get_pid().': Has children. Cannot edit';
            $this->stop = true;
        }
        else{
            $this->edited_label = html_entity_decode($this->object->label, ENT_QUOTES);
            if ($this->object->label == $this->edited_label){
                $this->stop = true;
                $this->message = $this->get_pid().': Label NOT edited';
            }
            else{
                $this->message = $this->get_pid().': Label edited';
            }
        }
    }
    
    protected function _exec(){
        $this->object->label = $this->edited_label;
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class show_type extends command{
    
    private function get_pub_type(AbstractObject $object) {
        $dom = new DOMDocument();
        $mods = $object['MODS']->content;
        if ($mods){
            $dom->loadXML($object['MODS']->content);
            $xpath = new DOMXPath($dom);
            $xpath->registerNamespace('m', 'http://loc.gov/mods/v3');
            
            
            $mods = $xpath->query('//mods:mods');
            $type = $xpath->evaluate('normalize-space(concat(mods:genre[@authorityURI="info:eu-repo/semantics"], " "))', $mods[0]);
        }
        else
            $type = 'WARNING: No MODS!!!';
        return $type;
        
    }
    protected function _dry_exec(){
        $this->message.= $this->get_pid().": ".$this->get_pub_type($this->object);
    }
    
    protected function _exec(){
        return 0;
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
abstract class batch_edit_mxds extends command{ 
    
    private   $mxds;
    protected   $dc;
    private   $mxds_content;
    protected   $dc_content;
    private   $dom;
    private   $xpath;
    protected $edited = false;
    protected $update_dc = false; //To be overridden in subclasses if DC update is required
    protected $mxds_type = ''; //To be overridden in subclasses
    protected $mxds_namespaceURI = ''; //To be overridden in subclasses
    protected $working_dir;
    
    protected function _dry_exec(){
//        $this->retrieve_islandora_object();
        if (in_array("ir:citationCModel", $this->object->models) or in_array("islandora:personCModel", $this->object->models)){
            $this->retrieve_mxds();
            $this->retrieve_mxds_content();
            $this->mxds_content = $this->edit_mxds(); //To be implemented in subclasses
            if ($this->mxds_content == $this->mxds->content){
                $this->edited = false;
                $this->message = $this->get_pid().": $this->mxds_type not edited";
            }
            else{
                $this->edited = true;
                $this->message = $this->get_pid().": $this->mxds_type edited";
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->mxds_content,$this->mxds_type.'_new');
                    $this->log_ds($this->mxds->content,$this->mxds_type.'_old');
                }
            }
            if ($this->update_dc && $this->edited){
                $this->edit_dc();
                $this->message.=', DC regenerated';
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->dc_content,'DC_new');
                    $this->log_ds($this->dc->content,'DC_old');
                }
            }
        }
        else{
            $this->message = $this->get_pid().': Not a valid object';
            $this->stop = true;
        }       
    }
    
    protected function _exec(){
        if ($this->edited == true){
            $this->ingest_mxds();
            if ($this->update_dc)
                $this->ingest_dc();
            $this->message.=" - Ingested";
        }
        else 
            $this->message.=" - Not ingested";
    }
    

    protected function get_mxds_content(){
        return $this->mxds_content;
    }
    
    protected function set_mxds_content($content){
        $this->mxds_content = $content;
    }
    
        
    protected function retrieve_mxds(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === $this->mxds_type){
                $this->mxds = $datastream;
                break;
            }
        }        
        //$this->retrieve_ds($this->mods, 'MODS');
    }

    protected function retrieve_mxds_content(){
        $this->mxds_content = $this->mxds->content;
    }
    
    abstract protected function edit_mxds();
    
    protected function ingest_mxds(){
        $this->mxds->content = $this->mxds_content;
    }

    private function retrieve_dc(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'DC'){
                $this->dc = $datastream;
                break;
            }
        }
        
//        $this->retrieve_ds($this->dc, 'DC');
    }
    
    protected function set_dom(){
        $this->dom = new DOMDocument('1.0');
        $this->dom->preserveWhiteSpace = false;
        $this->dom->formatOutput = true;
        $this->dom->loadXML($this->mxds_content);
    }
    
    protected function get_dom(){
        return $this->dom;
    }
    
    protected function set_xpath(){
        $this->xpath = new DOMXPath($this->dom);
        $this->xpath->registerNamespace(strtolower($this->mxds_type), $this->mxds_namespaceURI);
    }
    
    protected function get_xpath(){
        return $this->xpath;
    }
    
    abstract protected function get_mxds_to_dc();
    
    protected function edit_dc(){
        $this->retrieve_dc();
        $this->dc_content = static::runXSLTransform(array(
            'xsl' => $this->get_mxds_to_dc(),
//            'xsl' => drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl',
            'input' => $this->get_mxds_content(),
        ));
        
    }
    
    protected function ingest_dc(){
        if (!($this->dc_content === $this->dc->content)) {
            $this->dc->content = $this->dc_content;
        }
    }

    private static function runXSLTransform($info) {
        module_load_include('module', 'xml_form_builder', 'xml_form_builder');
        $xsl = new DOMDocument();   
        $xsl->load($info['xsl']);
        
        $input = new DOMDocument();
        $input->loadXML($info['input']);
        
        $processor = new XSLTProcessor();
        $processor->importStylesheet($xsl);
        
        $input = xml_form_builder_transform_mods_collection_hack($input);
        return $processor->transformToXML($input);
    }
    
    private function retrieve_ds($class_var, $ds_string){
        foreach ($this->object as $datastream) {
            if ($datastream->id === '$ds_string'){
                $class_var = $datastream;
                break;
            }
        }
    }  
    
    protected function log_ds($ds_content, $ds_name){
        $filename = date('Y-m-d_H-i-s').'_'.$this->pid.'_'.$ds_name;
        if ($this->options['wd'])
            $wd = $this->options['wd'].'/';
        else
            $wd = null;
        $fid = fopen($wd.$filename, 'w');
        fwrite($fid, $ds_content);
        fclose($fid);
    }
    
    protected function update_dc($upd_dc){
        $this->$update_dc = $upd_dc;
        return $this;
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
abstract class batch_edit_mods extends batch_edit_mxds{
    protected $mxds_type = 'MODS'; 
    protected $mxds_namespaceURI = 'http://loc.gov/mods/v3'; 
    
    function get_mxds_to_dc(){
//        return drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl';
        return drupal_get_path('module', 'xml_form_builder') . '/transforms/mods_to_dc.xsl';
    }
    
    function get_empty_mods(){
        $mods = new DOMDocument("1.0", "UTF-8");
        $mods->preserveWhiteSpace = false;
        $mods->formatOutput = true;
        $mods->loadXML('<mods xmlns="http://www.loc.gov/mods/v3" xmlns:mods="http://www.loc.gov/mods/v3" xmlns:xlink="http://www.w3.org/1999/xlink"/>');
        return $mods;
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
abstract class batch_edit_mads extends batch_edit_mxds{
    protected $mxds_type = 'MADS'; 
    protected $mxds_namespaceURI = '';

    function get_mxds_to_dc(){
        return drupal_get_path('module', 'zip_importer') . '/xsl/mads_to_dc.xsl';
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class edit_mads_xslt extends batch_edit_mads{

    protected function edit_mxds(){
        $this->update_dc = $this->options['update_dc'];
        $this->set_dom();
        return apply_xslt_to_mxds($this->get_dom(), $this->options['xslt']);
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class edit_mods_xslt extends batch_edit_mods{

    protected function edit_mxds(){
        $this->update_dc = $this->options['update_dc'];
        $this->set_dom();
        return apply_xslt_to_mxds($this->get_dom(), $this->options['xslt']);
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_publisher extends batch_edit_mods{
    protected $update_dc = true;
    
    protected function edit_mxds(){
        $this->set_dom();
        $this->set_xpath();
        $section = $this->get_xpath()->query('//mods:mods/mods:relatedItem [@type="host"]')[0];
        if($section){
            $publisher = $section->getElementsByTagName('publisher')->item(0)->nodeValue;
        }
        
        if ($publisher == $this->options['current']){
            $section->getElementsByTagName('publisher')->item(0)->nodeValue = $this->options['new'];
            return $this->get_dom()->saveXML();
        } 
        return $this->get_mxds_content();
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class first_name extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        
        $roles = explode(';',$this->options['roles']);
        
        $this->set_dom();
        $this->set_xpath();
        $edited = false;
        foreach ($roles as $role){
            $section = $this->get_xpath()->query("//mods:mods/mods:name [@type=\"personal\"  and mods:role/mods:roleTerm=\"$role\"]")[0];
            if($section && (empty($section->getAttribute('usage')))){
                $section->setAttribute('usage', 'primary');
                $edited = true;
            }
        }

        if ($edited)
            return $this->get_dom()->saveXML();
        else
            return $this->get_mxds_content();
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class append_add_info extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        $edited = false;
        
        $str = $this->options['string'];
        $this->set_dom();
        $this->set_xpath();
        $edited = false;
        
        $add_info_elem = $this->get_xpath()->query("//mods:mods/mods:note [@type=\"additional information\"]")->item(0);
        if ($add_info_elem === NULL){
            $add_info_elem = $this->get_dom()->createElement('note');
            $attribute = $this->get_dom()->createAttribute('type');
            $attribute->value = 'additional information';
            $add_info_elem->appendChild($attribute);
            $this->get_dom()->firstChild->appendChild($add_info_elem);
        }
                
        $add_info = $add_info_elem->nodeValue;
        
        if ($add_info != ""){
            $add_info .= ' ';
        }
        
        if (strpos($add_info, '#green') === FALSE) { //needs === to distinguish from 0
            $add_info .= '#green';
            $add_info_elem->nodeValue = $add_info;
            $edited = true;
        }

        if ($edited)
            return $this->get_dom()->saveXML();
            else
                return $this->get_mxds_content();
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_cit_dep_name extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        $this->set_dom();
        $this->set_xpath();
        $sections = $this->get_xpath()->query('//mods:mods/mods:name [@type="personal"]');
          
        $edited = false;
        $roles = array('author', 'thesis advisor', 'editor');
        $authors = [];
        foreach ($sections as $section){
#            if ($section->getElementsByTagName('roleTerm')->item(0)->nodeValue == 'author'){
            if (in_array($section->getElementsByTagName('roleTerm')->item(0)->nodeValue, $roles)){
                $affiliations = $section->getElementsByTagName('affiliation');
                foreach ($affiliations as $affiliation){
                    //->item(0)->nodeValue;
                    if (preg_replace('/\s+/', ' ', $affiliation->nodeValue) == $this->options['current']){
                        $affiliation->nodeValue = $this->options['new'];
                        $elements = $section->getElementsByTagName('nameIdentifier');
                        foreach ($elements as $element){
                            if ($element->getAttribute('type')=='authorId'){
                                array_push($authors, $element->nodeValue);
                            }
                        }
                        $edited = true;
                    }
                }
            }
        }
        
        $this->save_authors($authors);
        if ($edited)
            return $this->get_dom()->saveXML();
        else
            return $this->get_mxds_content();
    }
    
    private function save_authors($authors){
        $filename = $this->options['authors_file'];
        if ($this->options['wd'])
            $wd = $this->options['wd'].'/';
        else
            $wd = null;
        
        $fid = fopen($wd.$filename, 'a');
        foreach ($authors as $author){
            fwrite($fid, $author."\n");
        }
        fclose($fid);
    }    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_aut_dep_name extends batch_edit_mads{
    
    protected function edit_mxds(){
        $this->set_dom();
        $this->set_xpath();
        $sections = $this->get_xpath()->query('//mads:mads/mads:affiliation');
        
        $edited = false;
        foreach ($sections as $section){
            $affiliation = preg_replace('/\s+/', ' ',$section->getElementsByTagName('organization')->item(0)->nodeValue);
            if ($affiliation == $this->options['current']){
                $section->getElementsByTagName('organization')->item(0)->nodeValue = $this->options['new'];
                $edited = true;
            }
        }
        if ($edited)
            return $this->get_dom()->saveXML();
        else
            return $this->get_mxds_content();
    }   
}

/********************************************************************
 * Function to update the e-mail address in the MADS datastream of an author object (like wsl-authors:5672) where the
 * e-mail address just exists in the MADS, but not in the DC datastream (also there is only the Solr field MADS_email_m*).
 * 
 * Usage example: 
 *   $command_class = new update_author_email($pid, $dry, $command_options); 
 *   return($command_class->exec());
 *
 * Drush Example Command lines:
 *   drush -u 1 -r /var/www/html dora-batch --pids=single --pids_options='pid=eawag:1881' --command='update_author_email' --command_options='eMail=null' --dry=true &>/tmp/test-log.txt
 *   drush -u 1 -r /var/www/html dora-batch --pids=single --pids_options='pid=eawag:1881' --command='update_author_email' --command_options='eMail=some.one@example.com' --dry=false
 *
 ********************************************************************/
class update_author_email extends command{

	private function get_options() {
		$this->pid = empty($this->get_pid()) ? @rawurldecode($this->options['pid']) : $this->get_pid();
		$this->eMail = @trim($this->options['eMail']);
	}

	private function get_mads_content($pidGot) {
		if ( !( $obj = @islandora_object_load($pidGot) ) ) { return false; }
		if ( !( $dsContent = @$obj['MADS']->Content ) ) { return false; }
		return $dsContent;
	}

	private function setup_em_up(){
		$this->get_options();

		$this->stop = true;
		if ( !preg_match('/\w+:\d+/',$this->pid) || !$this->object ) {
			$this->message .= 'Error: PID missin"," Object invalid. ';
			return false;
		}
		if ( in_array(strtolower($this->eMail),array('null','none','delete','purge')) ) {
			$this->eMail = 'null';
		}
		elseif ( empty($this->eMail) ) {
			$this->message .= "Error: e-Mail address is empty (use keyword 'null' to remove it)! ";
			return false;
		}
		else { // We also may get an entire text list (separator hardly known) with multiple e-mail addressES:
			$eMatchAry = array();
			preg_match_all('/[a-z0-9]+@[a-z0-9]+\.{1}.[a-z]+/s',$this->eMail,$eMatchAry);
			if ( @sizeof($eMatchAry[0]) > 1 ) {
				if ( !preg_match('/[_a-z0-9-]+(\.[_a-z0-9-]+)*@[a-z0-9-]+(\.[a-z0-9-]+)*(\.[a-z]{2,9})/i',$this->eMail) ) {
					$this->message .= "Error: Oddity found in e-Mail addresses '" . $this->eMail . "'! ";
					return false;
				}
			} elseif ( !preg_match('/^[_a-z0-9-]+(\.[_a-z0-9-]+)*@[a-z0-9-]+(\.[a-z0-9-]+)*(\.[a-z]{2,9})$/i',$this->eMail) ) {
				$this->message .= "Error: Oddity found in e-Mail address '" . $this->eMail . "'! ";
				return false;
			}
		}
		if ( !@isset($this->object['MADS']) ) {
			$this->message .= 'Error: MADS datastream not defined or missing. ';
			return false;
		}
		$this->content = $this->get_mads_content($this->pid);	
		if ( $this->content == false ){
			$this->message .= 'Error: MADS datastream for '. $this->pid . ' is invalid! ';
			return false;
		}
		$this->stop = false;
		$this->message .= ' Success: got the MADS datastream for '. $this->pid . '! ';
	}

	protected function _dry_exec(){
		$this->stop = true;
		$this->setup_em_up();
	}

	protected function _exec(){			// will call _dry_exec()
		$this->stop = false;
		$this->sync_email_mads($this->eMail, $this->pid, $this->object, $this->content, TRUE);
	}

	// Intended that sync_email_mads() can be called from 'outside', for example:
	//	  module_load_include('inc', 'lib4ri_batch', 'includes/commands') OR include_once("/var/www/html/sites/all/drush/lib4ri_batch/includes/commands.inc")
	//	  $eMailTuner = new update_author_email("",false,"");
	//	  $eMailTuner->sync_email_mads( 'some.one@example.com', /* psi-authors:1234 */ '', $optionally_loaded_author_object );
	// Note:  - $eMailValue must *not* be empty, use keyword 'null' to remove an e-mail address.
	//        - This function does not give any status messages.
	public function sync_email_mads( $eMailValue = '', $pidNow = '', $object = NULL, $madsContent = '', $simpleCheck = FALSE ) {
		if ( $simpleCheck ) {
			if ( strtolower($eMailValue) != 'null' && !strpos($eMailValue,'@') ) { return false; } 
		}
		elseif ( strtolower($eMailValue) != 'null' && !preg_match('/^[_a-z0-9-]+(\.[_a-z0-9-]+)*@[a-z0-9-]+(\.[a-z0-9-]+)*(\.[a-z]{2,9})$/i',$eMailValue) ) {
			return false;
		}
		if ( $eMailValue == 'null' ) { $eMailValue = ''; }

		if ( ( $simpleCheck && strpos($pidNow,":") ) || ( !$simpleCheck && preg_match('/\w+:\d+/',$pidNow) ) ) {
			if ( !( $object = @islandora_object_load($pidNow) ) ) { return false; }
		}
		elseif ( $object == NULL ) {
			return false;
		}
		if ( stripos($madsContent,"<mads") === false && !( $madsContent = @$object['MADS']->content ) ) { return false; }

		$madsDOM = new DOMDocument("1.0", "UTF-8");
		$madsDOM->preserveWhiteSpace = false;
		$madsDOM->formatOutput = true;

		$madsDOM->loadXML($madsContent);
		$xpath = new DOMXPath($madsDOM);
		$xpath->registerNamespace('mads', 'http://www.loc.gov/mads/v2');

		$queryPath = "//mads:mads/mads:affiliation/mads:email";		// see: http://www.loc.gov/standards/mads/mads-outline-2-0.html
		$found1 = false;
		$edited = false;
		// assuming there could be more than one <email> element, going to update however just the first one found:
		foreach($xpath->query($queryPath) as $eMail) {
			if ( !$found1 && @isset($eMail->nodeValue) ) {
				$found1 = true;
			}
			if( !empty($eMailValue) ) {
				if ( !$edited && strcmp(strtolower($eMail->nodeValue),strtolower($eMailValue)) !== 0 ) {
					$eMail->nodeValue = $eMailValue;
					$edited = true;
				}
			}
			elseif ( !empty($eMail->nodeValue) ) {		// removing it + trying to clean up the XML
				$parent = $eMail->parentNode;
				$parent->removeChild($eMail);
				if ( $parent->hasChildNodes() == false ) {
					$parent->parentNode->removeChild($parent);	// to remove empty <affiliation/>
				}
				$edited = true;
			}
		}

		if( !$found1 && !empty($eMailValue) ) {
			if ( !( $root = @$madsDOM->getElementsByTagName('mads')->item(0) ) ) {
				$root = $madsDOM->createElement("mads");
				$madsDOM->appendChild($root);
			}
			if ( !( $affil = @$root->getElementsByTagName('affiliation')->item(0) ) ) {
				$affil = $madsDOM->createElement("affiliation");
				$root->appendChild($affil);
			}
			if ( $eMail = $madsDOM->createElement("email", $eMailValue) ) {
				$affil->appendChild($eMail);
				$edited = true;
			}
		}

		if ( $edited ) {
			$object['MADS']->content = $madsDOM->saveXML();
		}
	}
}

/********************************************************************
 * 
 * 
 * 
********************************************************************/
class test_edit extends batch_edit_mods{
    
    protected $update_dc = true;
    
    protected function edit_mxds(){
        return str_replace('bsgd','abc',  $this->get_mxds_content());
        
    }
}


/********************************************************************
 *
 *
 *
 ********************************************************************/
class cit_count_ingest extends command{
    private $did = 'CIT-DATA';
    private $max_size;
    
    private function set_max_size(){
        if (isset($this->options['max_size']))
            $this->max_size = $this->options['max_size'];
        else
            $this->max_size = 3;
    }
    
    protected function _dry_exec(){
        //        $this->retrieve_islandora_object();
        $this->set_max_size();
        if (!$this->object){
            $this->message .= $this->get_pid().': object not found';
            $this->stop = true;
        }
        else{
            $this->content=$this->get_content();    
            if (empty($this->content)){
                $this->message .= $this->get_pid().': XML file NOT found';
                $this->stop = true;
            }
            else {
                $this->message .= $this->get_pid().': XML file found; ';
                if ($this->object[$this->did])
                    $this->message .= sizeof($this->object[$this->did]).' datastream version(s) ';
            }       
        }
    }
    
    protected function _exec(){
        //Datastream purging
        if ($this->max_size == 0){
            $this->object->purgeDatastream($this->did);
            $this->message .= '- Datestream purged';
            return;
        }
        
        
        $cit_ds = $this->object[$this->did];        
        if ($cit_ds){
            $cit_ds->content = $this->content;
        }
        else {
            $cit_ds = $this->object->constructDatastream($this->did); //maybe $control_group is required
            $cit_ds->label = 'Citation related information';
            $cit_ds->content = $this->content;
            $this->object->ingestDatastream($cit_ds);
        }
        $this->message .= '- ingested';
        
        //Old ds deletion
        if (sizeof($cit_ds) > $this->max_size){
            $count = 0;
            $th = sizeof($cit_ds);
            for ($ii = $this->max_size; $ii < $th; $ii++){
                try {
                    unset($cit_ds[$this->max_size]);
                    $count++;
                }
                catch (Exception $e) {
                    $del_err = true;
                }
            }
            if ($del_error)
                $this->message .= ' - datastream deletion error(s)';
            $this->message .= ' - '.$count.' version(s) deleted, '.sizeof($cit_ds).' remaining'; 
        }

    }
    
    private function get_content(){
        $filename = $this->options['dirname']."/".str_replace(':', '_', $this->pid).'.xml';
        $content = file_get_contents($filename);
        return $content;
    }
    
    private function ingest_citcount($pid, $xml_data){
        $did = 'CIT-DATA';
        $object = islandora_object_load($pid);
        $cit_ds = $object[$did]; //Data stream selection
        
        if ($cit_ds){
            $cit_ds->content = $xml_data;
        }
        else {
            $cit_ds = $object->constructDatastream($did); //maybe $control_group is required
            $cit_ds->label = 'Citation related information';
            $cit_ds->content = $xml_data;
            $object->ingestDatastream($cit_ds);
        }
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class fetch_pub_ids extends batch_edit_mods{
    protected $update_dc = true;
    
    protected function edit_mxds(){
        module_load_include('inc', 'lib4ri_pub_ids', 'includes/utils');
        $this->set_dom();
        $this->set_xpath();
        $modified = add_ids_to_mods($this->get_dom(), $this->get_xpath());
        
        if ($modified){
            return $this->get_dom()->saveXML();
        }
        return $this->get_mxds_content();
     }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class regenerate_dc extends batch_edit_mods{
    protected $update_dc = true;
    
    protected function _dry_exec(){
        //        $this->retrieve_islandora_object();
        $this->edited = true;
        if (in_array("ir:citationCModel", $this->object->models) or in_array("islandora:personCModel", $this->object->models)){
            $this->retrieve_mxds();
            $this->retrieve_mxds_content();
            $this->message = $this->get_pid();
            if ($this->update_dc){
                $this->edit_dc();
                $this->message.=', DC regenerated';
                if ($this->options['log_ds'] == true){
                    $this->log_ds($this->dc_content,'DC_new');
                    $this->log_ds($this->dc->content,'DC_old');
                }
            }
        }
        else{
            $this->message = $this->get_pid().': Not a valid object';
            $this->stop = true;
        }
    }
    
    protected function _exec(){
            
            if ($this->update_dc)
                $this->ingest_dc();
            $this->message.=" - Ingested";
    }
    
    protected function edit_mxds(){
        /*required by the parent class but not used here*/
    }

}

/********************************************************************
 *
 * Intended to exectute any PHP code from a file on the server. It is recommended to avoid initial PHP tags in this file, just plain code.
 * Put this file here \\eaw-projects\Lib4RI_DORA_archive$\all\batch-work (Windows) resp. /var/www/html/data/batch-work/ (CentOS)
 *
 * Example :
 * drush -u 1 -r /var/www/html dora-batch --pids=single --command=process_code --command_options='file=code.txt|query(url)=x%3d789' --dry=false
 * 
 ********************************************************************/
class process_code extends command{

    protected function retrieve_islandora_object() {	// override
        $this->object = true;
    }  

	private function get_options() {
		$this->file = @trim( $this->options['file'] );			// file *path*
	}

	private function get_content($path) {
		if ( $content = @trim(file_get_contents($path)) ) {
			return $content;
		}
		return false;
	}

	private function setup_code(){
		$this->get_options();
		$this->content = $this->get_content($this->file);
		if ( $this->content == false ){
			$this->message .= " Error: Could not read file '" . $this->file . "', cancelling now!";
			return false;
		}
		$this->message .= " Notice: Found file '" . basename($this->file) . "', going to evaluate it...!";
		$this->stop = false;
	}

	protected function _dry_exec(){
		$this->stop = true;
		$this->setup_code();
	}

	protected function _exec(){
		$this->stop = false;
		$this->include_code($this->file,$this->content,$this->options);
	}

	protected function include_code( $incFile, $phpCode, $optAry = array() ) {
		// get varibles from command-options:
		foreach($optAry as $varName => $value) {
			$varName = trim($varName);
			if ( substr($varName,-5) == '(url)' ) {		// example: query(url)=x%3d789 becomes $query = 'x=789'
				$varName = rtrim(substr($varName,0,-5));
				$$varName = rawurldecode($value);
			}
			elseif ( !empty($varName) ) { $$varName = $value; }
		}
		// try to include the file once:
		if ( in_array(strrchr($incFile,'.'),array('module','php','inc')) ) {
			@include_once($incFile);
			return $incFile;
		}
		$tmpFile = ( @empty($optAry['tmpId']) ? ( date("Y-m-d.H-i-s.") . uniqid() ) : $optAry['tmpId'] );
		$tmpFile = '/tmp/batch-php-code.' . $tmpFile . '.inc';
		if ( stripos(substr(ltrim($phpCode),0,9),'<'.'?'.'php') === false && strpos(substr(rtrim($phpCode),-5),'?'.'>') === false ) {
			$phpCode = '<'.'?'.'php'."\r\n".trim($phpCode);
		}
		if ( @file_put_contents($tmpFile,$phpCode) && @filesize($tmpFile) ) {
			@include_once($tmpFile);
			return $tmpFile;
		}
		return FALSE;
	}
}

/********************************************************************
 *
 * class functions to add a PDF(A) datastream
 * based on class cit_count_ingest
 * Example :
 * drush -u 1 -r /var/www/html dora-batch --pids=single --pids_options='pid=eawag:679' --command=pdfa_ingest --command_options='pdfa=/tmp/eawag-679.PDF.1542369045.PDF-A.pdf|dsId=PDF_PDF-A|dsLabel=eawag-679.PDF.1542369045.PDF-A.pdf' --dry=false
 * 
 ********************************************************************/
class pdfa_ingest extends command{

	private $pidNow = "";
	private $pdfa = "";		/* = $file_uri */
	private $dsId = "";		/* = $datastream_id */
	private $dsLabel = "";		/* = $datastream_label */
	private $dsMime = "";
	private $dsRedo = FALSE;	/* = $force_fresh */

	private function get_content($file_path) {
		$content = file_get_contents($file_path);
		if ($content) {
			return $content;
		}
		$file_path = '/tmp/pdfa.'.strtr($this->get_pid(),":","-").'.'.time().'.pdf';
		return file_get_contents($file_path);
	}

	private function get_options_pdfa() {
		$this->pidNow = empty($this->get_pid()) ? $this->options['pid'] : $this->get_pid();
		$this->pdfa = $this->options['pdfa'];		/* = $file_uri */
		$this->dsId = $this->options['dsId'];		/* = $datastream_id */
		$this->dsLabel = $this->options['dsLabel'];		/* = $datastream_label */
		$this->dsMime = @empty($this->options['dsMime']) ? "application/pdf" : $this->options['dsMime'];		// there should be a auto-detection though...
		$this->dsRedo = @!isset($this->options['dsRedo']) ? FALSE : $this->options['dsRedo'];		/* = $force_fresh */
	}
				
	protected function _dry_exec(){
		$this->get_options_pdfa();
		if ( !$this->object || empty($this->pidNow) || empty($this->pdfa) || empty($this->dsId) || empty($this->dsLabel) ) {
			$this->message .= 'Error: Insufficient ' . strtoupper(substr(strrchr("/".($this->dsMime),"/"),1)) . ' or datastream parameters for ' . $pidNow . '. ';
			$this->stop = true;
			return;
		}
		$this->content = $this->get_content($this->pdfa);	
		if (empty($this->content)){
			$this->message .= 'Error: '. $this->pidNow . ' got a broken ' . strtoupper(substr(strrchr("/".($this->dsMime),"/"),1)) . '. ';
			$this->stop = true;
			return;
		}
		$this->message .= 'Success: ' . strtoupper(substr(strrchr("/".($this->dsMime),"/"),1)) . ' accepted for '. $this->pidNow . '. ';
	}

	protected function _exec(){
		$this->get_options_pdfa();
		if ( !$this->object || empty($this->pidNow) || empty($this->pdfa) || empty($this->dsId) || empty($this->dsLabel) ) {
			$this->message .= 'Error: Insufficient ' . strtoupper(substr(strrchr("/".($this->dsMime),"/"),1)) . ' or datastream settings for ' . $pidNow . '. ';
			$this->stop = true;
			return;
		}
		$dsItem = $this->object[$this->dsId];
		if ($dsItem){
			if (!$dsRedo) {
				return;
			}
			$this->object->purgeDatastream($this->dsId);
			$this->message .= 'Notice: ' . strtoupper(substr(strrchr("/".($this->dsMime),"/"),1)) . ' datestream purged to recreate it. ';
		}
		else {
			$dsItem = $this->object->constructDatastream($this->dsId);		// M is the (default) control_group
			$this->message .= 'Notice: New ' . strtoupper(substr(strrchr("/".($this->dsMime),"/"),1)) . ' datestream just created now. ';
		}
		$dsItem->label = $this->dsLabel;
		if ( @!empty($this->dsMime) ) { $dsItem->mimetype = $this->dsMime; }
		$dsItem->content = $this->get_content($this->pdfa);
		$this->object->ingestDatastream($dsItem);
	}
	
	private function ingest_pdfa(){
		$pidNow = empty($this->get_pid()) ? $this->options['pid'] : $this->get_pid();
		if ( $object = islandora_object_load($pidNow) ) {

			$pdfa = $this->options['pdfa'];		/* = $file_uri */
			$dsId = $this->options['dsId'];		/* = $datastream_id */
			$dsLabel = $this->options['dsLabel'];		/* = $datastream_label */
			$dsMime = @!isset($this->options['dsMime']) ? "" : $this->options['dsMime'];
		//	$dsRedo = @!isset($this->options['dsRedo']) ? FALSE : $this->options['dsRedo'];		/* = $force_fresh */

			$dsItem = $object->constructDatastream($dsId);		// M is the (default) control_group
			$dsItem->label = $dsLabel;
			if ( $dsMime != "" ) { $dsItem->mimetype = $dsMime; }
			$dsItem->content = get_content($pdfa);
			$object->ingestDatastream($dsItem);
		}
	}

}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class cit_count_detect extends command{
    
    private function get_mods_content(){
        $this->mods_content =  $this->object['MODS']->content;
    }
    
    private function get_id_labels(){
        return explode(';',$this->options['idlabels']);  
    }
    
    private function get_identifier($id_label){
        try{
            return $this->xpath->query("//mods:mods/mods:identifier [@type='$id_label']")->item(0)->nodeValue;
        }
        catch (Exception $e){
            return 0;
        }
    }
    
    protected function _dry_exec(){
        $this->dom = new DOMDocument();
        $this->get_mods_content();
        $this->dom->loadXML($this->mods_content);
        $this->xpath = new DOMXPath($this->dom);
        
        $this->ids_string = $this->get_pid().$this->options['idsep'];
        $this->identifiers = [];
        $id_labels = $this->get_id_labels();
        $this->stop = true;
        if ($this->mods_content){
            foreach ($id_labels as $id_label){
                $this->identifiers[$id_label] = $this->get_identifier($id_label);
                $this->ids_string .= $this->get_identifier($id_label).$this->options['idsep'];
                if (!empty($this->identifiers[$id_label]))
                    $this->stop = false;
            }
        }
        if ($this->stop == true){
            $this->message .= $this->get_pid().' NO IDs found';
        }
        else{
            $this->message .= $this->ids_string;
        }
    }

    protected function _exec(){
        $file_path = $this->options['wd'].'/'.$this->options['outfile'];
        $fid = fopen($file_path, 'a');        
        fwrite($fid, $this->ids_string."\n");
        fclose($fid);
    }
    
}

function apply_xslt_to_mxds($mxds, $path_to_xslt){
    
    $xslt_processor = new XSLTProcessor();
    $xslt_doc = new DOMDocument();
    $xslt_doc->load($path_to_xslt);
    $xslt_processor->importStylesheet($xslt_doc);
    $a = $xslt_processor->transformToDoc($mxds)->saveXML();
    var_dump($a);
    return $a;
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class get_mods extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
                
        return $this->get_mxds_content()."\n<downloaded />";
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class set_pdf_public extends command{
    private $rext;
    private $rint;
    private $ext_edited = FALSE;
    private $int_edited = FALSE;
    private $edited_int_content;
    private $edited_ext_content;
    
    
    protected function _dry_exec(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'RELS-INT'){
                $this->rint = $datastream;
            }
        }
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'RELS-EXT'){
                $this->rext = $datastream;
            }
        }
       
        $this->dom_int = new DOMDocument();
        $this->dom_int->loadXML($this->rint->content);
        
        $this->dom_ext = new DOMDocument();
        $this->dom_ext->loadXML($this->rext->content);

        $this->edited_int_content = $this->edit_rels_int();
        $this->edited_ext_content = $this->edit_rels_ext();
        
        if (/*$this->ext_edited &&*/ $this->int_edited){
            if ($this->options['log_ds'] == true){
                $this->log_ds($this->rint->content,'RELS-INT_old');
                $this->log_ds($this->rext->content,'RELS-EXT_old');
                $this->log_ds($this->edited_int_content,'RELS-INT_new');
                $this->log_ds($this->edited_ext_content,'RELS-EXT_new');
            }
            $this->message = $this->get_pid().": Edited";
        }
        else{
            $this->message = $this->get_pid().": Not edited";
        }
        
    }

    protected function _exec(){
        if (/*($this->ext_edited == true) && */($this->int_edited == true)){
            $this->rint->content = $this->edited_int_content;
            $this->rext->content = $this->edited_ext_content;
            $this->message.=" - Ingested";
        }
        else{
            $this->message.=" - Not ingested";
        }
    
    }

    private function edit_rels_int(){
        $xpath = new DOMXPath($this->dom_int);
        $items = $xpath->query("//rdf:RDF/rdf:Description");
        foreach ($items as $item){
            $attr_value = $item->getAttribute('rdf:about');
            if ((strpos($attr_value, 'PDF')) && (strpos($attr_value, 'PDF-A')===FALSE)){
                if ($item->getElementsByTagName('lib4ridora-multi-embargo-document_version')->item(0)->nodeValue == 'published version'){                    
                    $item->getElementsByTagName('lib4ridora-multi-embargo-availability')->item(0)->nodeValue = 'public';
                    $embargo_date = $item->getElementsByTagName('lib4ridora-multi-embargo-embargo_date')->item(0);
                    if ($embargo_date){
                        $item->removeChild($embargo_date);
                    }
                    
                    $this->int_edited = TRUE;
                }
            }
        }
        
        $ret = '';
        foreach ($this->dom_int->childNodes as $node) {
            $ret .= $this->dom_int->saveXML($node, LIBXML_NOEMPTYTAG);
        }
        return $ret."\n";
    }
    
    private function edit_rels_ext(){
        $xpath = new DOMXPath($this->dom_ext);
        $item = $xpath->query("//rdf:RDF/rdf:Description/islandora:fullText")->item(0);
        $item->nodeValue = 'Open Access';
        
        $ret = '';
        foreach ($this->dom_ext->childNodes as $node) {
            $ret .= $this->dom_ext->saveXML($node, LIBXML_NOEMPTYTAG);
        }
        return $ret."\n";
        
    }
    
 
    private function log_ds($ds_content, $ds_name){
        $filename = date('Y-m-d_H-i-s').'_'.$this->pid.'_'.$ds_name;
        if ($this->options['wd'])
            $wd = $this->options['wd'].'/';
            else
                $wd = null;
                $fid = fopen($wd.$filename, 'w');
                fwrite($fid, $ds_content);
                fclose($fid);
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class edit_r_ext extends command{
    private $rext;
    private $ext_edited = FALSE;
    private $edited_ext_content;
    
    protected function _dry_exec(){
        foreach ($this->object as $datastream) {
            if ($datastream->id === 'RELS-EXT'){
                $this->rext = $datastream;
            }
        }
                
        $this->dom_ext = new DOMDocument();
        $this->dom_ext->loadXML($this->rext->content);
        
        $this->edited_ext_content = $this->edit_rels_ext();
        
        if ($this->ext_edited /*&& $this->int_edited*/){
            if ($this->options['log_ds'] == true){
                $this->log_ds($this->rext->content,'RELS-EXT_old');
                $this->log_ds($this->edited_ext_content,'RELS-EXT_new');
            }
            $this->message = $this->get_pid().": Edited";
        }
        else{
            $this->message = $this->get_pid().": Not edited";
        }
        
    }
    
    protected function _exec(){
        if ($this->ext_edited /*&& $this->int_edited*/){
            $this->rext->content = $this->edited_ext_content;
            $this->message.=" - Ingested";
        }
        else{
            $this->message.=" - Not ingested";
        }
        
    }
    
    private function edit_rels_ext(){
        $xpath = new DOMXPath($this->dom_ext);
        $item = $xpath->query("//rdf:RDF/rdf:Description/islandora:inheritXacmlFrom")->item(0);
        $item->setAttribute('rdf:resource','info:fedora/'.$this->options['collection']);
        $this->ext_edited = TRUE;
        $ret = '';
        foreach ($this->dom_ext->childNodes as $node) {
            $ret .= $this->dom_ext->saveXML($node, LIBXML_NOEMPTYTAG);
        }
        return $ret."\n";
        
    }
    
    
    private function log_ds($ds_content, $ds_name){
        $filename = date('Y-m-d_H-i-s').'_'.$this->pid.'_'.$ds_name;
        if ($this->options['wd'])
            $wd = $this->options['wd'].'/';
            else
                $wd = null;
                $fid = fopen($wd.$filename, 'w');
                fwrite($fid, $ds_content);
                fclose($fid);
    }
    
}

/********************************************************************
 *
 *
 *
 ********************************************************************/

class edit_owner extends command{
    protected function _dry_exec(){
        //        $this->retrieve_islandora_object();
        if (!$this->object){
            $this->message = $this->get_pid().': object not found';
            $this->stop = true;
        }
        elseif ($this->has_children()){
            $this->message = $this->get_pid().': Has children. Cannot edit';
            $this->stop = true;
        }
        else{
            $this->message = $this->get_pid().': Editing...';
        }
    }
    
    protected function _exec(){
        $this->object->owner=$this->options['owner'];
        $this->message.=' Owner edited';
    }
    
}



/********************************************************************
 *
 *
 *
 ********************************************************************/
//use the calss first_name as model
class psi_affiliation extends batch_edit_mods{
    protected $update_dc = false; // there is no affiliation in dc
    
    protected function edit_mxds(){
        
        $this->set_dom();
        $this->set_xpath();
        $edited = false;
        // ADD here the logic to change the MODS and set $edited to true if edited
        
        
        ///////////////////////////////////////////////////////////////////////////
        
        if ($edited)
            return $this->get_dom()->saveXML();
            else
                return $this->get_mxds_content();
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class upd_facilities extends batch_edit_mods{
    protected $update_dc = true;
    
    protected function edit_mxds(){
        
        $current = $this->options['current'];
        $new = $this->options['new'];
        
        $this->set_dom();
        $this->set_xpath();
        $edited = false;
        
        $facilities = $this->get_xpath()->query("//mods:mods/mods:extension/mods:researchFacilities/mods:researchFacility");
        foreach ($facilities as $facility){
            $fac = $facility->getElementsByTagName('facility');
            
            if ($new === ''){
                if ($fac->item(0)->nodeValue == $current){
                    $facility->parentNode->removeChild($facility);
                    $edited = true;
                }
            }
            
            else{
                if ($fac->item(0)->nodeValue == $current){
                    $fac->item(0)->nodeValue = $new;
                    $edited = true;
                }
            }
            
        }
                
        if ($edited)
            return $this->get_dom()->saveXML();
        else
            return $this->get_mxds_content();
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class doi_check extends command{
    private function resolve(AbstractObject $object) {
        
        $mods = $object['MODS']->content;
        if ($mods){
            $dom = new DOMDocument();
            $dom->loadXML($mods);
            $xpath = new DOMXPath($dom);
            $doi_node = $xpath->query(variable_get('lib4ri_pub_ids_doi_mods'));
            
            if ($doi_node){
                $doi = trim($doi_node->item(0)->nodeValue);
                $ch = curl_init('https://dx.doi.org/'.$doi);
                curl_setopt($ch, CURLOPT_HEADER, 1);
                curl_setopt($ch, CURLOPT_NOBODY, 1);
                curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
                $headers = curl_exec($ch);
                curl_close($ch);
                            
                if (preg_match('/^Location: (.+)$/im', $headers, $matches)){
                    return "Resolved - DOI: $doi - ".trim($matches[0]);
                }
                
                if (preg_match('/^HTTP.* 400/', $headers, $matches_http_code) || preg_match('/^HTTP.* 404/', $headers, $matches_http_code)){
                    return "NOT resolved - DOI: $doi ";
                }
                
                return "Status UNKWNOWN";
            }
            else{
                return "DOI not found in MODS";
            }
        }
        else
            $type = 'WARNING: No MODS!!!';
       return $type;
            
    }
    protected function _dry_exec(){
        $this->message.= $this->get_pid().": ".$this->resolve($this->object);
    }
    
    protected function _exec(){
        return 0;
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class delete_mods_tag extends batch_edit_mods{ //
    protected $update_dc = FALSE; 
    
    protected function edit_mxds(){
        $edited = false;
        $tag_name = $this->options['tag-name'];
        $type = $this->options['type'];
        if ($this->options['upd_dc']){
            $this->update_dc($this->options['upd_dc']);
        }
        
        $this->set_dom();
        $this->set_xpath();
        $query = "mods:$tag_name";
        if (!is_null($type)){
            $query .= " [@type= \"${type}\"]";
        }
        
        $items = $this->get_xpath()->query($query);
        
        foreach ($items as $item){
            $item->parentNode->removeChild($item);
            $edited = true;
        }
        
        if ($edited){
            return $this->get_dom()->saveXML();
        }
        else{
            return $this->get_mxds_content();
        }
    }

}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class delete_mods_element extends batch_edit_mods{
    protected function edit_mxds(){
        
    }
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_quality_control extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        
        $edited = false;
        $initials = array("jhb","lk","nk","ln","cb","vmv","sl","sg","nh","mb","cv","tk","wn","sco","cro","so","ps","nc","sgi","jl","nci","lm","jb","sge","cm","ub","if","sc");
        $initials_ok = [];
        
        $this->set_dom();
        $this->set_xpath();
        $section_dep = $this->get_xpath()->query('//mods:mods/mods:note [@type="department descriptor"]')->item(0);
        
        /************************************************************************/
//         foreach ($initials as $initial){
//             if(stripos($section_dep->nodeValue, $initial) !== false){
//                 array_push($initials_ok, $initial);
//             }
//         }
        $string = $section_dep->nodeValue;
        $string = str_replace(',',' ',$string);
        $string = str_replace(';',' ',$string);
        $string = str_replace('\\',' ',$string);
        $string = preg_replace('!\s+!', ' ', $string);
        $initials_ok = array_intersect($initials, array_map('strtolower',array_map('trim',explode(' ',$string))));
 //       var_dump(array_map('strtolower',array_map('trim',explode(' ',$string))));
                
        if ((sizeof($initials_ok) != 0)){
            $section_qc = $this->get_xpath()->query('//mods:mods/mods:note [@type="quality_control"]')->item(0);
            $section_qc_id = $this->get_xpath()->query('//mods:mods/mods:note [@type="quality_control_ID"]')->item(0);
            
            $initials_ok_string = implode(',', $initials_ok );
            
            if ((sizeof($initials_ok) == 1) && $initials_ok_string == 'cro'){
                $qc_value = '';
            }
            else{
                $qc_value = 'Yes';
            }
            
            if ($section_qc === NULL){
                $section_qc = $this->get_dom()->createElement('note');
                $attribute = $this->get_dom()->createAttribute('type');
                $attribute->value = 'quality_control';
                $section_qc->appendChild($attribute);
                $section_qc->nodeValue = $qc_value;
                $this->get_dom()->firstChild->appendChild($section_qc);
            }
            else{
                $section_qc->nodeValue = $qc_value; 
            }
    
            if ($section_qc_id === NULL){
                $section_qc_id = $this->get_dom()->createElement('note');
                $attribute = $this->get_dom()->createAttribute('type');
                $attribute->value = 'quality_control_ID';
                $section_qc_id->appendChild($attribute);
                $section_qc_id->nodeValue = $initials_ok_string;
                $this->get_dom()->firstChild->appendChild($section_qc_id);
            }
            else{
                $section_qc_id->nodeValue = $initials_ok_string; 
            }
            
            $edited = true;
        }
        
        /************************************************************************/
        if((stripos($section_dep->nodeValue, 'jt') !== false) && (sizeof($initials_ok) == 0)){
            $section_notes = $this->get_xpath()->query('//mods:mods/mods:note [@type="notes_lib4ri"]')->item(0);
            
            if ($section_notes === NULL){
                $section_notes = $this->get_dom()->createElement('note');
                $attribute = $this->get_dom()->createAttribute('type');
                $attribute->value = 'notes_lib4ri';
                $section_notes->appendChild($attribute);
                $section_notes->nodeValue = $section_notes->nodeValue.' jt';
                $this->get_dom()->firstChild->appendChild($section_qc_id);
            }
            else{
                $section_notes->nodeValue = $section_notes->nodeValue.' jt';
            }
            
            $edited = true;
        }
        
        
        /************************************************************************/
        if ($edited){
            $section_dep->nodeValue = '';
            return $this->get_dom()->saveXML();
        }
        else{
            return $this->get_mxds_content();
        }
    }
}
/********************************************************************
 *
 *
 *
 ********************************************************************/
class duplicate_page_edit extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        $to_remove = $this->options['remove'];
        $edited = false;
        $this->set_dom();
        $this->set_xpath();
        
        $section = array('page' => NULL, 'pages' => NULL);
        
        $section['page']  = $this->get_xpath()->query('//mods:mods/mods:relatedItem/mods:part/mods:extent [@unit="page"]' )->item(0);
        $section['pages'] = $this->get_xpath()->query('//mods:mods/mods:relatedItem/mods:part/mods:extent [@unit="pages"]')->item(0);
        
        if (($section['page'] === NULL) OR ($section['pages'] === NULL)){
            return $this->get_mxds_content();
        }
        else{
            $section[$to_remove]->parentNode->removeChild($section[$to_remove]);
            $edited = true;
            return $this->get_dom()->saveXML();
        }
    }            
}

/********************************************************************
 *
 *
 *
 ********************************************************************/
class append_funding_info extends batch_edit_mods{
    protected $update_dc = false;
    
    protected function edit_mxds(){
        module_load_include('inc', 'lib4ri_pub_db', 'utils/classes');
        module_load_include('inc', 'lib4ri_pub_db', 'utils/data_servant_classes');
        
        $edited = false;
        $this->set_dom();
        $this->set_xpath();
   
        $doi = $this->get_xpath()->query('//mods:mods/mods:identifier [@type="doi"]' )->item(0)->nodeValue;
        $eid = $this->get_xpath()->query('//mods:mods/mods:identifier [@type="scopus"]' )->item(0)->nodeValue;
        
        $retriever = new $this->options['retriever'];
        $retriever->setDoi($doi);
        if (!empty($eid)){
            $retriever->setEid($eid);
        }

        $retriever->setKey(variable_get('lib4ri_pub_db_scopus_api_key'))->serve();
        $retrieved_mods = $retriever->getProcessedDom()[0];

        if (is_null($retrieved_mods)){
            $retrieved_mods = $this->get_empty_mods();
        }
        $enricher = new $this->options['enricher'];
        $enricher->setIds(array('doi'=>$doi, 'eid'=>$eid));
        $enricher->setDom($retrieved_mods);
        $enricher->enrich();
        $enriched_mods = $enricher->getDom();

        $awards_in_dora = $this->get_awards_in_mods($this->get_dom()->saveXML());
        $awards_fetched = $this->get_awards_in_mods($enriched_mods->saveXML());  
        $awards_to_add = array_diff($awards_fetched, $awards_in_dora);
        if (!empty($awards_to_add)){
            $edited = true;
            $funding_section = $this->get_xpath()->query('//mods:mods/mods:extension/mods:fundingReferences' )->item(0);
            if (empty($funding_section)){
                $extension_section = $this->get_xpath()->query('//mods:mods/mods:extension' )->item(0);
                if (empty($extension_section)){
                    $extension_section = $this->get_dom()->createElement('extension');
                    $mods_root = $this->get_xpath()->query('//mods:mods')[0];
                    $mods_root->appendChild($extension_section);
                }
                $funding_section = $this->get_dom()->createElement('fundingReferences');
                $extension_section->appendChild($funding_section);
            }
            //Remove empty reference sections
            $mods_fund_references = $this->get_xpath()->query('//mods:mods/mods:extension/mods:fundingReferences/mods:fundingReference' );
            foreach ($mods_fund_references as $reference){
                if( ($reference->getElementsByTagName('awardNumber')->item(0)->nodeValue == null) && ($reference->getElementsByTagName('awardTitle')->item(0)->nodeValue == null)){
                    $funding_section->removeChild($reference);
                }
                
            }

            foreach ($awards_to_add as $award){           
                $funding_reference = $this->get_funding_reference($enriched_mods->saveXML(), $award);             
                $funding_section->appendChild($this->get_dom()->importNode($funding_reference, true));
            }
            return $this->get_dom()->saveXML();
        }
        return $this->get_mxds_content();
    }
    
    private function get_awards_in_mods($mods){
        $mods_dom = new DOMDocument("1.0", "UTF-8");
        $mods_dom->preserveWhiteSpace = false;
        $mods_dom->formatOutput = true;
        $mods_dom->loadXML($mods);
        
        $xpath = new DOMXPath($mods_dom);
        $xpath->registerNamespace(strtolower($this->mxds_type), $this->mxds_namespaceURI);

        $funding_references = $xpath->query('//mods:mods/mods:extension/mods:fundingReferences/mods:fundingReference');
        $awards =[];
        foreach ($funding_references as $reference){
            array_push($awards, $reference->getElementsByTagName('awardNumber')->item(0)->nodeValue);
        }
        
        return $awards;
    }
    
    private function get_funding_reference($xml, $award_number){
        $dom = new DOMDocument("1.0", "UTF-8");
        $dom->preserveWhiteSpace = false;
        $dom->formatOutput = true;
        $dom->loadXML($xml);
        
        $xpath = new DOMXPath($dom);
        $xpath->registerNamespace(strtolower($this->mxds_type), $this->mxds_namespaceURI);
        
        $funding_references = $xpath->query('//mods:mods/mods:extension/mods:fundingReferences/mods:fundingReference');        
        foreach ($funding_references as $reference){
            if (($reference->getElementsByTagName('awardNumber')->item(0)->nodeValue) == $award_number){
                return $reference;
            }
        }
        
        return null;
    }
}

    
/********************************************************************
 *
 *
 *
 ********************************************************************/
class update_datacite_alert extends command{
    protected $upd;
    
    protected function _dry_exec(){
        module_load_include('inc', 'lib4ri_datacite_alert', 'includes/classes');
        
        $dom = new DOMDocument();
        $dom->loadXML($this->object['MODS']->content);
        $xpath = new DOMXPath($dom);        
        $doi = $xpath->query('//mods:mods/mods:identifier [@type="doi"]' )->item(0)->nodeValue;
       
        if (trim($doi) == ''){
            return -1;
        }
        
        $this->upd = new DataCiteRetrieval();
        $this->upd->setDefQuery();
        $this->upd->setDoi($doi);
        $this->upd->setDoraPid($this->get_pid());
        $this->upd->fetch()->filterSingleDoi();
        
        $this->message .=  $this->get_pid().': '.count($this->upd->getToAdd()).' items found to add';
    }

    protected function _exec(){
        if (count($this->upd->getToAdd()) > 0){
            $this->upd->update();
            $this->message .=  ' - added to alert list';
        }
    }

    
}
    