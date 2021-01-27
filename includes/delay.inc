<?php
abstract class delay{
    
    public function __construct($options) {
        $this->options = $options;
    }
    
    abstract public function pause(); 

}

class none extends delay{
    public function pause($pid){
    }
}

class fix_delay extends delay{
    public function pause($pid){
        sleep((double)$this->options['delay']);
    }    
}

