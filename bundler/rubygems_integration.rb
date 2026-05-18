require "bundler"

module Bundler
  class RubygemsIntegration
    def gem_dir
      Gem.dir
    end

    def gem_bindir
      Gem.bindir
    end

    def default_path
      Gem.path
    end

    def loaded_specs(name = nil)
      specs = Gem.loaded_specs
      return specs if name.nil?
      specs[name]
    end
  end
end
