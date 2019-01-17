require 'rjb'
Rjb::load
begin
  Rjb::import('java.lang.Integer').parseInt('x')
rescue => e
  begin
    raise e
  rescue => f
    if e.class == f.class
      puts "I expect the equality to be true"
    else
      puts "Unexpectedly the re-raised Java exception has changed " +
           "from a #{e.class} into a #{f.class}"
    end
  end
end

