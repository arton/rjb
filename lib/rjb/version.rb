module Rjb
  class << self

    private

    # @return [String, nil] the valid version number. Can be nil.
    def read_version
      path = File.expand_path('../../../ext/rjb.c', __FILE__)
      File.open(path) do |f|
        f.each_line do |l|
          m = /RJB_VERSION\s+"(.+?)"/.match(l)

          # The file is closed even in this case.
          return m[1] if m
        end
      end
    end
  end

  # The `Rjb` module defines `VERSION` in the C code.
  # If Rjb is already required we have the constant.
  unless defined?(::Rjb::VERSION)
    unless (VERSION = read_version)
      raise 'Cannot find a valid version number in `rjb.c`!'
    end
  end
end
