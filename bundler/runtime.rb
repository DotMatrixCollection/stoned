require "bundler"

module Bundler
  class Runtime
    def initialize(definition, settings)
      @definition = definition
      @settings = settings
    end

    def definition
      @definition
    end

    def settings
      @settings
    end

    def setup(*groups)
      Bundler.setup(*groups)
      self
    end

    def require(*groups)
      Bundler.require(*groups)
      self
    end
  end
end
