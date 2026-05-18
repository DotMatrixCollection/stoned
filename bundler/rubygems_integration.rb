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

    def platforms
      Gem.platforms
    end

    def loaded_specs(name = nil)
      specs = Gem.loaded_specs
      return specs if name.nil?
      specs[name]
    end

    def all_specs
      specs = []
      Gem::Specification.each { |spec| specs << spec }
      specs
    end

    def installed_specs
      all_specs
    end

    def find_name(name)
      all_specs.select { |spec| spec.name == name.to_s }
    end
  end
end
