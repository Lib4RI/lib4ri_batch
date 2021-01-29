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
            $load = getServerLoad((double)$this->options['delay']);
            echo "$load ... waiting\n";
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
    
    private function getServerLoad($delay)
    {
        $load = null;
        
        if (is_readable("/proc/stat"))
        {
            // Collect 2 samples - each with 1 second period
            // See: https://de.wikipedia.org/wiki/Load#Der_Load_Average_auf_Unix-Systemen
            $statData1 = sample_stat();
            sleep(1);
            $statData2 = sample_stat();
            
            if
            (
                (!is_null($statData1)) &&
                (!is_null($statData2))
                )
            {
                // Get difference
                $statData2[0] -= $statData1[0];
                $statData2[1] -= $statData1[1];
                $statData2[2] -= $statData1[2];
                $statData2[3] -= $statData1[3];
                
                // Sum up the 4 values for User, Nice, System and Idle and calculate
                // the percentage of idle time (which is part of the 4 values!)
                $cpuTime = $statData2[0] + $statData2[1] + $statData2[2] + $statData2[3];
                
                // Invert percentage to get CPU time, not idle time
                $load = 100 - ($statData2[3] * 100 / $cpuTime);
            }
        }
            
        return $load;
    }
}