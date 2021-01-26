<?php
abstract class delay{
    
    public function __construct($pid, $options) {
        $this->pid = $pid;
        $this->options = $options;
    }
    
    abstract public function pause(); 

}

class none extends delay{
    public function pause(){
    }
}

class fix_delay extends delay{
    public function pause(){
        sleep((double)$this->options['delay']);
    }    
}

