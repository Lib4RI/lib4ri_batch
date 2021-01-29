<?php
abstract class delay{
    
    public function __construct($options) {
        $this->options = $options;
    }
    
    abstract public function pause($pid); 

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

class threshold_delay extends delay{
    public function pause($pid){
        $load = $this->options['threshold'];
        
        while ($load >= $this->options['threshold']){
            
        }
        
    }
    
    private function sample_stat(){
        if (is_readable("/proc/stat"))
        {
            $stats = @file_get_contents("/proc/stat");
            
            if ($stats !== false)
            {
                // Remove double spaces to make it easier to extract values with explode()
                $stats = preg_replace("/[[:blank:]]+/", " ", $stats);
                
                // Separate lines
                $stats = str_replace(array("\r\n", "\n\r", "\r"), "\n", $stats);
                $stats = explode("\n", $stats);
                
                // Separate values and find line for main CPU load
                foreach ($stats as $statLine)
                {
                    $statLineData = explode(" ", trim($statLine));
                    
                    // Found!
                    if
                    (
                        (count($statLineData) >= 5) &&
                        ($statLineData[0] == "cpu")
                        )
                    {
                        return array(
                            $statLineData[1],
                            $statLineData[2],
                            $statLineData[3],
                            $statLineData[4],
                        );
                    }
                }
            }
        }
        
        return null;
    }
}